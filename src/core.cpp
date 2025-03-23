module;
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>
#include <variant>
#include <memory>
#include <cstring>
#include <deque>
module evk;
import :core;
import :utils;
using namespace evk;

namespace
{
    struct VkStruct {
        vk::StructureType sType;
        void* pNext;
    };
}

void Queue::submitAndWaitIdle(vk::ArrayProxy<const vk::SubmitInfo> const& submits, const vk::Fence fence) const
{
	submit(submits, fence);
	waitIdle();
}

void Queue::submit2AndWaitIdle(vk::ArrayProxy<const vk::SubmitInfo2> const& submits, const vk::Fence fence) const
{
	submit2(submits, fence);
	waitIdle();
}

Device::Device(
    const evk::SharedPtr<Instance>& instance,
    const vk::raii::PhysicalDevice& physicalDevice,
    const std::vector<const char*>& extensions,
    const Queues& queues,
    void* pNext
) : vk::raii::Device{ nullptr }, physicalDevice{ physicalDevice }, memoryProperties{ physicalDevice.getMemoryProperties() }
{
    _instance = instance;
    const auto prop = physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceSubgroupProperties, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR, vk::PhysicalDeviceAccelerationStructurePropertiesKHR, vk::PhysicalDeviceDescriptorBufferPropertiesEXT>();
    properties = prop.get<vk::PhysicalDeviceProperties2>().properties;
    subgroupProperties = prop.get<vk::PhysicalDeviceSubgroupProperties>();
    rayTracingPipelineProperties = prop.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    accelerationStructureProperties = prop.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
	descriptorBufferProperties = prop.get<vk::PhysicalDeviceDescriptorBufferPropertiesEXT>();

    constexpr float priority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> deviceQueueCreateInfos;
    deviceQueueCreateInfos.reserve(queues.size());
    for (const auto& [queueFamilyIndex, queueCount] : queues) {
        deviceQueueCreateInfos.emplace_back(vk::DeviceQueueCreateInfo{ {}, queueFamilyIndex, queueCount, &priority });
    }
    const vk::DeviceCreateInfo deviceCreateInfo{ {}, deviceQueueCreateInfos, {}, extensions,{}, pNext };
    vk::raii::Device::operator=({ physicalDevice, deviceCreateInfo });

    // get all our queues -> queue[family][index]
    if (queues.empty()) throw std::invalid_argument{ "No queue indices specified" };
    _queues.resize(physicalDevice.getQueueFamilyProperties().size());
    for (const auto& [queueFamilyIndex, queueCount] : queues) {
        _queues[queueFamilyIndex] = std::vector<Queue>{ queueCount, {nullptr} };
        for (uint32_t i = 0; i < queueCount; ++i) _queues[queueFamilyIndex][i] = evk::Queue{ vk::raii::Device::getQueue(queueFamilyIndex, i) };
    }

    auto* p = static_cast<VkStruct*>(pNext);
    while(p) {
		if (p->sType == vk::StructureType::ePhysicalDeviceAccelerationStructureFeaturesKHR) {
            const vk::PhysicalDeviceAccelerationStructureFeaturesKHR* s = reinterpret_cast<vk::PhysicalDeviceAccelerationStructureFeaturesKHR*>(p);
            hasAccelerationStructureActive = s->accelerationStructure;
		}
		p = static_cast<VkStruct*>(p->pNext);
	}
}

std::optional<uint32_t> Device::findMemoryTypeIndex(const vk::MemoryRequirements& requirements, const vk::MemoryPropertyFlags propertyFlags) const
{
    return utils::findMemoryTypeIndex(memoryProperties, requirements, propertyFlags);
}

bool Device::imageFormatSupported(const vk::Format format, const vk::ImageType type, const vk::ImageTiling tiling, const vk::ImageUsageFlags usage, const vk::ImageCreateFlags flags) const
{
    try {
	    auto _ = physicalDevice.getImageFormatProperties(format, type, tiling, usage, flags);
		return true;
	} catch (const vk::SystemError&) {
		return false;
	}
}

const Queue& Device::getQueue(const QueueFamily queueFamily, const QueueCount queueIndex) const
{
    if (queueFamily >= _queues.size()) throw std::out_of_range( "Queue family index out of range" );
    if (queueIndex >= _queues[queueFamily].size()) throw std::out_of_range("Queue index out of range" );
    return _queues[queueFamily][queueIndex];
}

const Queue& Device::getQueue(const std::pair<QueueFamily, QueueCount>& queueIndices) const
{
    return getQueue(queueIndices.first, queueIndices.second);
}

CommandPool::CommandPool(
    const evk::SharedPtr<Device>& device,
    const Device::QueueFamily queueFamily,
    const vk::CommandPoolCreateFlags flags
) : Resource{ device }, flags{ flags }, queueFamily{ queueFamily }, commandPool{ *dev, { flags, queueFamily } } {}

vk::raii::CommandBuffer CommandPool::allocateCommandBuffer(vk::CommandBufferLevel level) const
{
    auto cb = vk::raii::CommandBuffers{ *dev, { *commandPool, level, 1 } };
    return std::move(cb[0]);
}

Buffer::Buffer() : Resource{ nullptr }, buffer{ nullptr }, memory{ nullptr }, deviceAddress{ 0 }, size{ 0 } {}
Buffer::Buffer(
    const evk::SharedPtr<Device>& device,
    vk::DeviceSize size,
    const vk::BufferUsageFlags usageFlags,
    const vk::MemoryPropertyFlags memoryPropertyFlags
) : Resource{ device }, buffer{ *dev, { {}, size, usageFlags } }, memory{ nullptr }, deviceAddress{ 0 }, size{ size }, _usageFlags{ usageFlags }, _memoryPropertyFlags{ memoryPropertyFlags }
{
    const auto memoryRequirements = buffer.getMemoryRequirements();
    const auto memoryTypeIndex = dev->findMemoryTypeIndex(memoryRequirements, _memoryPropertyFlags);
    if (!memoryTypeIndex.has_value()) throw std::runtime_error{ "No memory type index found" };
    constexpr vk::MemoryAllocateFlagsInfo memoryAllocateFlagsInfo{ vk::MemoryAllocateFlagBits::eDeviceAddress };
    const vk::MemoryAllocateInfo memoryAllocateInfo{ memoryRequirements.size, memoryTypeIndex.value(), &memoryAllocateFlagsInfo };
    memory = vk::raii::DeviceMemory{ *dev, memoryAllocateInfo };
    buffer.bindMemory(*memory, 0);

    if (_usageFlags & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        const vk::BufferDeviceAddressInfo bufferDeviceAddressInfo{ *buffer };
        deviceAddress = dev->getBufferAddress(bufferDeviceAddressInfo); /* for bindless rendering */
    }
}

void Buffer::resize(const vk::DeviceSize& s)
{
    if (s == size) return;
    size = s;
    buffer = vk::raii::Buffer{ *dev, { {}, size, _usageFlags } };
    const auto memoryRequirements = buffer.getMemoryRequirements();
    const auto memoryTypeIndex = dev->findMemoryTypeIndex(memoryRequirements, _memoryPropertyFlags);
    if (!memoryTypeIndex.has_value()) throw std::runtime_error{ "No memory type index found" };
    constexpr vk::MemoryAllocateFlagsInfo memoryAllocateFlagsInfo{ vk::MemoryAllocateFlagBits::eDeviceAddress };
    const vk::MemoryAllocateInfo memoryAllocateInfo{ memoryRequirements.size, memoryTypeIndex.value(), &memoryAllocateFlagsInfo };
    memory = vk::raii::DeviceMemory{ *dev, memoryAllocateInfo };
    buffer.bindMemory(*memory, 0);

    if (_usageFlags & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        const vk::BufferDeviceAddressInfo bufferDeviceAddressInfo{ *buffer };
        deviceAddress = dev->getBufferAddress(bufferDeviceAddressInfo); /* for bindless rendering */
    }
}

Image::Image(
    const evk::SharedPtr<Device>& device,
    const vk::Extent3D extent,
    const vk::Format format,
    const vk::ImageTiling tiling,
    const vk::ImageUsageFlags usageFlags,
    const vk::MemoryPropertyFlags memoryPropertyFlags
) : Resource{ device }, image{ nullptr }, imageView{ nullptr }, memory{ nullptr }, format{ format }, 
    aspectMask{ utils::formatToAspectMask(format) }, _tiling{ tiling }, _usageFlags{ usageFlags }, _memoryPropertyFlags{ memoryPropertyFlags }
{
    resize(extent);

    //imageViewAddressProperties = imageView.getAddressNVX();

    //{
    //    auto properties = device->physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceHostImageCopyPropertiesEXT>();
    //    auto hostImageCopyProperties = properties.get<vk::PhysicalDeviceHostImageCopyPropertiesEXT>();

    //    auto imageFormatInfo = vk::PhysicalDeviceImageFormatInfo2{ format, imageType, tiling, usageFlags, {} };
    //    //auto formatProperties = device->physicalDevice.getImageFormatProperties2<vk::ImageFormatProperties2, vk::HostImageCopyDevicePerformanceQueryEXT>(imageFormatInfo);
    //    //auto hostImageCopyDevicePerformanceQuery = formatProperties.get<vk::HostImageCopyDevicePerformanceQueryEXT>();

    //    int x = 2;
    //}
}

void Image::resize(vk::Extent3D ex)
{
    const vk::ImageType imageType = utils::extentToImageType(ex);
    const vk::ImageViewType imageViewType = utils::extentToImageViewType(ex);

    extent = ex;
    if (extent.height == 0) extent.height = 1;
    if (extent.depth == 0) extent.depth = 1;
    const vk::ImageCreateInfo imageCreateInfo{ {}, imageType, format, extent,
        1, 1, vk::SampleCountFlagBits::e1, _tiling,
        _usageFlags
    };
    image = vk::raii::Image{ *dev, imageCreateInfo };
    const auto memoryRequirements = dev->getImageMemoryRequirements2({ *image }).memoryRequirements;
    const auto memoryTypeIndex = dev->findMemoryTypeIndex(memoryRequirements, _memoryPropertyFlags);
    if (!memoryTypeIndex.has_value()) throw std::runtime_error{ "No memory type index found" };
    constexpr vk::MemoryAllocateFlagsInfo memoryAllocateFlagsInfo{ vk::MemoryAllocateFlagBits::eDeviceAddress };
    const vk::MemoryAllocateInfo memoryAllocateInfo{ memoryRequirements.size, memoryTypeIndex.value(), &memoryAllocateFlagsInfo };
    memory = vk::raii::DeviceMemory{ *dev, memoryAllocateInfo };
    image.bindMemory(*memory, 0);

    imageView = vk::raii::ImageView{ *dev, vk::ImageViewCreateInfo{ {}, *image, imageViewType, format,
        {}, { aspectMask, 0, 1, 0, 1 } } };
    barrier.oldLayout = vk::ImageLayout::eUndefined;
    barrier.image = *image;
    barrier.subresourceRange = { aspectMask, 0, 1, 0, 1 };
}

void Image::transitionLayout(const vk::ImageLayout newLayout)
{
    const vk::ImageSubresourceRange imageSubresourceRange{ aspectMask, 0, 1, 0, 1 };
    const vk::HostImageLayoutTransitionInfoEXT hostImageLayoutTransitionInfoEXT{ *image, barrier.oldLayout, newLayout, imageSubresourceRange };
    dev->transitionImageLayoutEXT(hostImageLayoutTransitionInfoEXT);
    barrier.oldLayout = newLayout;
}
void Image::copyMemoryToImage(const void* ptr) const
{
    const auto memoryToImageCopy = vk::MemoryToImageCopyEXT{ ptr }
        .setImageExtent(extent)
        .setImageSubresource({ aspectMask, 0, 0, 1 });
    const vk::CopyMemoryToImageInfoEXT copyMemoryToImageInfo{ {}, *image, barrier.oldLayout, memoryToImageCopy };
    dev->copyMemoryToImageEXT(copyMemoryToImageInfo);
}
void Image::copyImageToMemory(void* ptr) const
{
    const auto imageToMemoryCopy = vk::ImageToMemoryCopyEXT{ ptr }
        .setImageExtent(extent)
        .setImageSubresource({ aspectMask, 0, 0, 1 });
    const vk::CopyImageToMemoryInfoEXT copyImageToMemoryInfo{ {}, *image, barrier.oldLayout, imageToMemoryCopy };
    dev->copyImageToMemoryEXT(copyImageToMemoryInfo);
}

DescriptorSetLayout::DescriptorSetLayout(
    const evk::SharedPtr<Device>& device,
    const Bindings& bindings
) : Resource{ device }, _bindings{ bindings }, layout{ nullptr }
{
    // layout
    std::vector<vk::DescriptorSetLayoutBinding> layoutBinding(bindings.size());
    std::vector<vk::DescriptorBindingFlags> bindingFlags(bindings.size());
    for (size_t i = 0; i < bindings.size(); ++i) {
        layoutBinding[i] = bindings[i].first;
        bindingFlags[i] = bindings[i].second;
    }
    vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo{ bindingFlags };
    const vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{ {}, layoutBinding, &bindingFlagsCreateInfo };
    layout = vk::raii::DescriptorSetLayout{ *dev, layoutCreateInfo };
}

void DescriptorSet::setDescriptor(const uint32_t binding, const Descriptor& data, const uint32_t index)
{
    std::visit([&]<typename T>(const T & v) {
        if constexpr (std::is_same_v<T, vk::DescriptorImageInfo>) {
            std::get<std::vector<vk::DescriptorImageInfo>>(_descriptors[binding])[index] = v;
        }
        else if constexpr (std::is_same_v<T, vk::DescriptorBufferInfo>) {
            std::get<std::vector<vk::DescriptorBufferInfo>>(_descriptors[binding])[index] = v;
        }
        else if constexpr (std::is_same_v<T, vk::BufferView>) {
            std::get<std::vector<vk::BufferView>>(_descriptors[binding])[index] = v;
        }
        else if constexpr (std::is_same_v<T, vk::AccelerationStructureKHR>) {
            std::get<std::vector<vk::AccelerationStructureKHR>>(_descriptors[binding])[index] = v;
        }
        else throw std::runtime_error("Descriptor type not supported");
    }, data);
}

void DescriptorSet::update()
{
    std::vector<vk::WriteDescriptorSet> descWrite;
    descWrite.reserve(_bindings.size());
    std::deque<vk::WriteDescriptorSetAccelerationStructureKHR> writeAs;
    for (const auto& binding : _bindings) {
        vk::WriteDescriptorSet write{ set, binding.first.binding, 0, 0, binding.first.descriptorType };

        std::visit([&write, &writeAs]<typename T>(const T & data) {
            if constexpr (std::is_same_v<T, std::vector<vk::DescriptorImageInfo>>) {
                for (const auto& imageInfo : data) {
                    if (imageInfo.imageView) write.descriptorCount++;
                    else break;
                }
                write.setPImageInfo(data.data());
            }
            else if constexpr (std::is_same_v<T, std::vector<vk::DescriptorBufferInfo>>) {
                for (const auto& bufferInfo : data) {
                    if (bufferInfo.buffer) write.descriptorCount++;
                    else break;
                }
                write.setPBufferInfo(data.data());
            }
            else if constexpr (std::is_same_v<T, std::vector<vk::BufferView>>) {
                for (const auto& bufferView : data) {
                    if (bufferView) write.descriptorCount++;
                    else break;
                }
                write.setPTexelBufferView(data.data());
            }
            else if constexpr (std::is_same_v<T, std::vector<vk::AccelerationStructureKHR>>) {
                for (const auto& as : data) {
                    if (as) write.descriptorCount++;
                    else break;
                }
                writeAs.emplace_back(write.descriptorCount, data.data());
                write.setPNext(&writeAs.back());
            }
            else throw std::runtime_error("Descriptor type not supported");
        }, _descriptors[binding.first.binding]);

        if (write.descriptorCount) descWrite.push_back(write);
    }
    dev->updateDescriptorSets(descWrite, {});
}

ShaderSpecialization::ShaderSpecialization(
    const std::vector<vk::SpecializationMapEntry>& entries,
    const void* data
) : _entries{ entries }
{
    size_t size = 0;
    for (const auto& entry : _entries) size = std::max(size, entry.offset + entry.size);
    _data = std::vector<uint8_t>(size, 0);
    std::memcpy(_data.data(), data, _data.size());
    constInfo = vk::SpecializationInfo{ static_cast<uint32_t>(_entries.size()), _entries.data(), _data.size(), _data.data() };
}

ShaderObject::ShaderObject(
    const evk::SharedPtr<Device>& device,
    const std::vector<ShaderStage>& shaderStages,
    const std::vector<vk::PushConstantRange>& pcRanges,
    const ShaderSpecialization& specialization,
    const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts
) : Resource{ device }, shaders{ shaderStages.size(), nullptr }, stages{ shaderStages.size() }, layout{ *dev, vk::PipelineLayoutCreateInfo{}.setPushConstantRanges(pcRanges).setSetLayouts(descriptorSetLayouts) } {
    std::vector shaderCreateInfos{ shaderStages.size(), vk::ShaderCreateInfoEXT{ shaderStages.size() > 1u ? vk::ShaderCreateFlagBitsEXT::eLinkStage : vk::ShaderCreateFlagsEXT{} }
        .setCodeType(vk::ShaderCodeTypeEXT::eSpirv).setPushConstantRanges(pcRanges).setPSpecializationInfo(&specialization.constInfo).setSetLayouts(descriptorSetLayouts) };

    _spvs.reserve(shaderStages.size());
    for (size_t i = 0; i < shaderStages.size(); ++i) {
        stages[i] = std::get<0>(shaderStages[i]);
        _spvs.emplace_back(std::get<1>(shaderStages[i]).begin(), std::get<1>(shaderStages[i]).end());
        shaderCreateInfos[i].setStage(std::get<0>(shaderStages[i]));
        shaderCreateInfos[i].setPName(std::get<2>(shaderStages[i]).data());
        if (i < (shaderStages.size() - 1)) shaderCreateInfos[i].setNextStage(std::get<0>(shaderStages[i + 1u]));
        shaderCreateInfos[i].setCode<uint32_t>(std::get<1>(shaderStages[i]));
    }
    _shaders = dev->createShadersEXT(shaderCreateInfos);
    for (size_t i = 0; i < shaderStages.size(); ++i) shaders[i] = *_shaders[i]; // needed in order to pass the vector directly to bindShadersEXT()
}
