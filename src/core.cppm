module;
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <string_view>
#include <variant>
#include <map>
#include <stdexcept>
#include <utility>
export module evk:core;
import :utils;
import vulkan_hpp;

export namespace evk
{
    EVK_API void checkResult(vk::Result result, const char* message)
    {
        if (result != vk::Result::eSuccess) throw std::runtime_error(message);
    }

    EVK_API const vk::raii::Context& context()
    {
	    static const vk::raii::Context ctx;
		return ctx;
    }

    // only here for single line creation
    struct Instance : vk::raii::Instance
    {
        EVK_API Instance() : vk::raii::Instance{ nullptr } {}
        EVK_API Instance(
            const vk::raii::Context& ctx,
            vk::InstanceCreateFlags flags,
            const vk::ApplicationInfo& appInfo,
            const std::vector<const char*>& layers,
            const std::vector<const char*>& extensions,
            const void* pNext = nullptr) : vk::raii::Instance{ ctx, { flags, &appInfo, layers, extensions} } {}
    };

    struct Queue : vk::raii::Queue
    {
	    EVK_API void submitAndWaitIdle(vk::ArrayProxy<const vk::SubmitInfo> const& submits, vk::Fence fence) const;
	    EVK_API void submit2AndWaitIdle(vk::ArrayProxy<const vk::SubmitInfo2> const& submits, vk::Fence fence) const;
    };

    struct Device : vk::raii::Device
    {
        using QueueFamily = uint32_t;
        using QueueCount = uint32_t;
        using Queues = std::unordered_map<QueueFamily, QueueCount>;

        EVK_API Device() : vk::raii::Device{ nullptr }, physicalDevice{ nullptr }{}
        EVK_API Device(
            const vk::raii::PhysicalDevice& physicalDevice,
            const std::vector<const char*>& extensions,
            const Queues& queues,
            void* pNext = nullptr
        );

        EVK_API [[nodiscard]] std::optional<uint32_t> findMemoryTypeIndex(
            const vk::MemoryRequirements& requirements, 
            vk::MemoryPropertyFlags propertyFlags
        ) const;

        EVK_API [[nodiscard]] bool imageFormatSupported(
            vk::Format format,
            vk::ImageType type,
            vk::ImageTiling tiling,
            vk::ImageUsageFlags usage,
            vk::ImageCreateFlags flags = {}
        ) const;
        
        EVK_API const Queue& getQueue(
            QueueFamily queueFamily, 
            QueueCount queueIndex = 0
        );

        EVK_API const Queue& getQueue(
            const std::pair<QueueFamily, QueueCount>& queueIndices
        );

        EVK_API operator const vk::raii::PhysicalDevice& () const { return physicalDevice; }

        std::vector<std::vector<Queue>> _queues;
        vk::raii::PhysicalDevice physicalDevice;
		// extra properties
        vk::PhysicalDeviceMemoryProperties memoryProperties;
        vk::PhysicalDeviceProperties properties;
        vk::PhysicalDeviceSubgroupProperties subgroupProperties;
        vk::PhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties;
        vk::PhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties;
		vk::PhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProperties;
        // has
        bool hasAccelerationStructureActive = false;
    };

    // Every resource has a device reference
    struct Resource { std::shared_ptr<Device> dev; };

    struct CommandPool : Resource
    {
        EVK_API CommandPool(
            const std::shared_ptr<Device>& device,
            Device::QueueFamily queueFamily,
            vk::CommandPoolCreateFlags flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer
        );

        EVK_API [[nodiscard]] vk::raii::CommandBuffer allocateCommandBuffer(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) const;

        vk::CommandPoolCreateFlags flags;
        Device::QueueFamily queueFamily;
        vk::raii::CommandPool commandPool;
    };

    // namespace Memory {
    //     EVK_API constexpr vk::MemoryPropertyFlags devLocal = vk::MemoryPropertyFlagBits::eDeviceLocal;
    //     EVK_API constexpr vk::MemoryPropertyFlags devLocalHostVisible = vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible;
    //     EVK_API constexpr vk::MemoryPropertyFlags devLocalHostVisibleCached = vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCached;
    // }
    struct Buffer : Resource
    {
        EVK_API Buffer();
        EVK_API Buffer(
            const std::shared_ptr<Device>& device,
            vk::DeviceSize size,
            vk::BufferUsageFlags usageFlags,
            vk::MemoryPropertyFlags memoryPropertyFlags
        );
        vk::raii::Buffer buffer;
        vk::raii::DeviceMemory memory;
        vk::DeviceAddress deviceAddress;
        vk::DeviceSize size;
    };

    struct Image : Resource
    {
        EVK_API Image() : Resource{ nullptr }, image{ nullptr }, imageView{ nullptr }, memory{ nullptr }, format{ vk::Format::eUndefined }, layout{ vk::ImageLayout::eUndefined } {}
        EVK_API Image(
            const std::shared_ptr<Device>& device,
            vk::Extent3D extent,
            vk::Format format,
            vk::ImageTiling tiling,
            vk::ImageUsageFlags usageFlags,
            vk::MemoryPropertyFlags memoryPropertyFlags
        );

        EVK_API void transitionLayout(vk::ImageLayout newLayout);
        EVK_API void copyMemoryToImage(const void* ptr) const;
        EVK_API void copyImageToMemory(void* ptr) const;

        vk::raii::Image image;
        vk::raii::ImageView imageView;
        vk::raii::DeviceMemory memory;
        vk::ImageViewAddressPropertiesNVX imageViewAddressProperties;

        vk::Extent3D extent;
        vk::Format format;
        vk::ImageAspectFlags aspectMask;
        vk::ImageLayout layout;
    };

    struct MutableDescriptorSetLayout : Resource
    {
        EVK_API MutableDescriptorSetLayout() : Resource{ nullptr }, layout{ nullptr }, descriptorCount{ 0 } {}
        EVK_API MutableDescriptorSetLayout(
            const std::shared_ptr<Device>& device,
            const uint32_t descriptorCount,
			const vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eAll,
            const vk::DescriptorBindingFlags bindingFlags = {},
			const vk::DescriptorSetLayoutCreateFlags layoutCreateFlags = {},
			const std::vector<vk::DescriptorType>& descriptorTypes = { vk::DescriptorType::eSampler, vk::DescriptorType::eCombinedImageSampler, vk::DescriptorType::eSampledImage, vk::DescriptorType::eStorageImage, vk::DescriptorType::eUniformTexelBuffer, vk::DescriptorType::eStorageTexelBuffer, vk::DescriptorType::eUniformBuffer, vk::DescriptorType::eStorageBuffer }
        ) : Resource{ device }, layout{ nullptr }, descriptorCount{ descriptorCount }
        {
            vk::MutableDescriptorTypeListEXT mutableTypes{ descriptorTypes };
            vk::MutableDescriptorTypeCreateInfoEXT mutableCreateInfo{ mutableTypes };
        	vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo{ { bindingFlags }, &mutableCreateInfo };

        	vk::DescriptorSetLayoutBinding layoutBinding{ 0, vk::DescriptorType::eMutableEXT, descriptorCount, stages };
            const vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{ layoutCreateFlags, layoutBinding, &bindingFlagsCreateInfo };
            layout = vk::raii::DescriptorSetLayout{ *dev, layoutCreateInfo };
        }

        EVK_API operator const vk::DescriptorSetLayout& () const { return *layout; }
        vk::raii::DescriptorSetLayout layout;
		uint32_t descriptorCount;
    };

    struct MutableDescriptorSet : Resource
    {
        EVK_API MutableDescriptorSet() : Resource{ nullptr }, pool{ nullptr }, set{ nullptr } {}
        EVK_API MutableDescriptorSet(
            const std::shared_ptr<Device>& device,
            const MutableDescriptorSetLayout& layout,
            const vk::DescriptorPoolCreateFlags& poolFlags = {},
			const void* pNext = nullptr
        ) : Resource{ device }, pool{ nullptr }, set{ nullptr }
        {
			// pool
	        const vk::DescriptorPoolSize poolSize { vk::DescriptorType::eMutableEXT, layout.descriptorCount };
            const vk::DescriptorPoolCreateInfo descPoolInfo{ poolFlags, 1, poolSize };
            pool = vk::raii::DescriptorPool{ *dev, descPoolInfo };
            // set
            //const vk::DescriptorSetVariableDescriptorCountAllocateInfo varDescCountAllocInfo = { 1, &bindings.back().first.descriptorCount };
            const vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo{ *pool, *layout.layout, nullptr };
            set = dev->allocateDescriptorSets(descriptorSetAllocateInfo)[0].release();
        }
        EVK_API operator const vk::DescriptorSet& () const { return set; }

        vk::raii::DescriptorPool pool;
        vk::DescriptorSet set;
    };

    struct DescriptorSetLayout : Resource
    {
        struct Binding : std::pair<vk::DescriptorSetLayoutBinding, vk::DescriptorBindingFlags>
        {
            EVK_API Binding(const vk::DescriptorSetLayoutBinding& binding, vk::DescriptorBindingFlags flags = {}) : std::pair<vk::DescriptorSetLayoutBinding, vk::DescriptorBindingFlags>{ binding, flags } {}
		};
        using Bindings = std::vector<Binding>;
        EVK_API DescriptorSetLayout() : Resource{ nullptr }, layout{ nullptr } {}
        EVK_API DescriptorSetLayout(
            const std::shared_ptr<Device>& device,
            const Bindings& bindings
        );

        EVK_API vk::DeviceSize sizeInBytes() const { return layout.getSizeEXT(); }
        EVK_API vk::DeviceSize bindingOffsetInBytes(const uint32_t binding) const { return layout.getBindingOffsetEXT(binding); }

        EVK_API operator const vk::DescriptorSetLayout& () const { return *layout; }

        Bindings _bindings;
        vk::raii::DescriptorSetLayout layout;
    };

    struct DescriptorSet : Resource
    {
        struct Binding : std::pair<vk::DescriptorSetLayoutBinding, vk::DescriptorBindingFlags>
        {
            EVK_API Binding(const vk::DescriptorSetLayoutBinding& binding, vk::DescriptorBindingFlags flags = {}) : std::pair<vk::DescriptorSetLayoutBinding, vk::DescriptorBindingFlags>{ binding, flags } {}
		};
        using Bindings = std::vector<Binding>;
        using Descriptor = std::variant<vk::DescriptorImageInfo, vk::DescriptorBufferInfo, vk::BufferView, vk::AccelerationStructureKHR>;
        using Descriptors = std::variant<std::vector<vk::DescriptorImageInfo>, std::vector<vk::DescriptorBufferInfo>, std::vector<vk::BufferView>, std::vector<vk::AccelerationStructureKHR>>;
        EVK_API DescriptorSet() : Resource{ nullptr }, layout{ nullptr }, pool{ nullptr }, set{ nullptr } {}
    	EVK_API DescriptorSet(
            const std::shared_ptr<Device>& device,
            const Bindings& bindings
        ) : Resource{ device }, _bindings{ bindings }, layout{ nullptr }, pool{ nullptr }, set{ nullptr }
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

            // pool
            std::vector<vk::DescriptorPoolSize> poolSizes(bindings.size());
            for (size_t i = 0; i < bindings.size(); i++) poolSizes[i].setType(bindings[i].first.descriptorType).setDescriptorCount(bindings[i].first.descriptorCount);
            const vk::DescriptorPoolCreateInfo descPoolInfo{ {}, 1, poolSizes };
            pool = vk::raii::DescriptorPool{ *dev, descPoolInfo };

            // descriptor set
            const vk::DescriptorSetVariableDescriptorCountAllocateInfo varDescCountAllocInfo = { 1, &bindings.back().first.descriptorCount };
			const vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo{ *pool, *layout, &varDescCountAllocInfo };
            set = dev->allocateDescriptorSets(descriptorSetAllocateInfo)[0].release();

            // make room for descriptors
            for (size_t i = 0; i < bindings.size(); ++i)
            {
	            if (bindings[i].first.descriptorType == vk::DescriptorType::eSampler || bindings[i].first.descriptorType == vk::DescriptorType::eCombinedImageSampler ||
                    bindings[i].first.descriptorType == vk::DescriptorType::eSampledImage || bindings[i].first.descriptorType == vk::DescriptorType::eStorageImage)
				{
                    _descriptors.emplace(bindings[i].first.binding, std::vector<vk::DescriptorImageInfo>{ bindings[i].first.descriptorCount });
				}
            }
        }

        EVK_API void setDescriptor(uint32_t binding, const Descriptor& data, uint32_t index = 0);

        EVK_API void update()
        {
            std::vector<vk::WriteDescriptorSet> descWrite;
            descWrite.reserve(_bindings.size());
            for (const auto& binding : _bindings) {
                vk::WriteDescriptorSet write { set, binding.first.binding, 0, 0, binding.first.descriptorType};
                if (const auto& imageInfoVector = std::get_if<std::vector<vk::DescriptorImageInfo>>(&_descriptors[binding.first.binding])) {
                    for (const auto& imageInfo : *imageInfoVector) {
                        if (imageInfo.imageView) write.descriptorCount++;
                        else break;
                    }
                    write.setPImageInfo(imageInfoVector->data());
                }
                if(write.descriptorCount) descWrite.push_back(write);
            }
            dev->updateDescriptorSets(descWrite, {});
        }

        EVK_API operator const vk::DescriptorSetLayout& () const { return *layout; }
        EVK_API operator const vk::DescriptorSet& () const { return set; }

        Bindings _bindings;
        std::map<uint32_t, Descriptors> _descriptors;
        vk::raii::DescriptorSetLayout layout;
        vk::raii::DescriptorPool pool;
        vk::DescriptorSet set;
    };

    struct ShaderSpecialization
    {
        EVK_API ShaderSpecialization() = default;
        EVK_API ShaderSpecialization(
            const std::vector<vk::SpecializationMapEntry>& entries,
            const void* data
        );

	    std::vector<vk::SpecializationMapEntry> _entries;
        std::vector<uint8_t> _data;
		vk::SpecializationInfo constInfo;
	};

    struct Shader : Resource, vk::raii::ShaderModule
    {
        EVK_API Shader() : Resource{ nullptr }, vk::raii::ShaderModule{ nullptr } {}
        EVK_API Shader(
            const std::shared_ptr<Device>& device,
            const vk::ShaderStageFlagBits stage,
            const std::vector<uint32_t>& spv
        ) : Resource{ device }, vk::raii::ShaderModule{ *device, { {}, spv } }, stage{ stage }
	    {}
        vk::ShaderStageFlagBits stage;
    };
    // Shader, entryPoint
    using ShaderModules = std::vector<std::pair<std::reference_wrapper<const evk::Shader>, std::string_view>>;

    // stage, spv, entryPoint
    using ShaderStage = std::tuple<const vk::ShaderStageFlagBits, const std::reference_wrapper<const std::vector<uint32_t>>, std::string_view>;

    struct ShaderObject : Resource
    {
        EVK_API ShaderObject() : Resource{ nullptr }, layout{ nullptr } {}
        EVK_API ShaderObject(
            const std::shared_ptr<Device>& device,
            const std::vector<ShaderStage>& shaderStages,
            const std::vector<vk::PushConstantRange>& pcRanges = {},
            const ShaderSpecialization& specialization = {},
            const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts = {}
        );

        std::vector<std::vector<uint32_t>> _spvs;
        std::vector<vk::raii::ShaderEXT> _shaders;
        std::vector<vk::ShaderEXT> shaders;
        std::vector<vk::ShaderStageFlagBits> stages;
        vk::raii::PipelineLayout layout;
    };

    struct Swapchain : Resource
    {
        // Data for one frame/image in our swapchain
        struct Frame {
            EVK_API Frame(const vk::raii::Device& device, const vk::Image& image, const vk::Format format, vk::raii::CommandBuffer& commandBuffer) : image{ image }, imageView{ nullptr },
                inFlightFence{ device, vk::FenceCreateInfo{ vk::FenceCreateFlagBits::eSignaled } }, submitFence{ device, vk::FenceCreateInfo{ vk::FenceCreateFlagBits::eSignaled } },
        		nextImageAvailableSemaphore{ device, vk::SemaphoreCreateInfo{} }, renderFinishedSemaphore{ device, vk::SemaphoreCreateInfo{} }, commandBuffer{ std::move(commandBuffer) }
            {
                imageView = vk::raii::ImageView{ device, vk::ImageViewCreateInfo{ {}, image, vk::ImageViewType::e2D, format,
                    {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } } };
            }
            vk::Image image;
            vk::raii::ImageView imageView;
            vk::raii::Fence inFlightFence, submitFence;
            vk::raii::Semaphore nextImageAvailableSemaphore /* refers to the next frame */, renderFinishedSemaphore /* refers to current frame */;
            vk::raii::CommandBuffer commandBuffer;
        };

        EVK_API Swapchain() : Resource{ nullptr }, currentImageIdx{ 0 }, previousImageIdx{ 0 }, imageCount{ 0 }, extent{ 0, 0 }, swapchainKHR{ nullptr }, commandPool{ nullptr } {}
        EVK_API Swapchain(const std::shared_ptr<Device>& device, const vk::SwapchainCreateInfoKHR& createInfo, const uint32_t queueFamilyIndex) : Resource{ device }, currentImageIdx{ 0 }, previousImageIdx{ 0 },
            swapchainKHR{ nullptr }, commandPool{ *dev, { vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queueFamilyIndex } }
        {
            imageCount = createInfo.minImageCount;
            extent = createInfo.imageExtent;
            swapchainKHR = vk::raii::SwapchainKHR{ *dev, createInfo };

            currentImageIdx = imageCount - 1u; // just for init
            auto commandBuffers = vk::raii::CommandBuffers{ *dev, { *commandPool, vk::CommandBufferLevel::ePrimary, imageCount } };
            const std::vector<vk::Image> images = swapchainKHR.getImages();
            frames.reserve(imageCount);
            for (uint32_t i = 0; i < imageCount; ++i) frames.emplace_back(*dev, images[i], createInfo.imageFormat, commandBuffers[i]);
        }
        EVK_API ~Swapchain() { for (auto& frame : frames) checkResult(dev->waitForFences({ *frame.inFlightFence, *frame.submitFence }, vk::True, UINT64_MAX), "waiting for fence error"); }

        EVK_API void acquireNextImage() {
            const Frame& oldFrame = getCurrentFrame();
            checkResult(dev->waitForFences(*oldFrame.inFlightFence, vk::True, UINT64_MAX), "waiting for fence error");
            dev->resetFences(*oldFrame.inFlightFence);
            const std::pair<vk::Result, uint32_t> nextImage = swapchainKHR.acquireNextImage(0, *oldFrame.nextImageAvailableSemaphore, *oldFrame.inFlightFence);
            checkResult(nextImage.first, "acquiring next swapchain image error");
            previousImageIdx = currentImageIdx;
            currentImageIdx = nextImage.second;

            const Frame& newFrame = getCurrentFrame();
            checkResult(dev->waitForFences(*newFrame.submitFence, vk::True, UINT64_MAX), "waiting for fence error");
            dev->resetFences(*newFrame.submitFence);
            newFrame.commandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        }

        EVK_API void submitImage(const vk::raii::Queue& presentQueue, const vk::PipelineStageFlags waitDstStageMask = vk::PipelineStageFlagBits::eNone) {
            const Frame& curFrame = getCurrentFrame();
            curFrame.commandBuffer.end();

            presentQueue.submit(vk::SubmitInfo{ *getPreviousFrame().nextImageAvailableSemaphore, waitDstStageMask,
                *curFrame.commandBuffer, *curFrame.renderFinishedSemaphore }, curFrame.submitFence);
            checkResult(presentQueue.presentKHR({ *curFrame.renderFinishedSemaphore, *swapchainKHR, currentImageIdx }), "present swapchain image error");
        }

        EVK_API const Frame& getCurrentFrame() { return frames[currentImageIdx]; }
        EVK_API const Frame& getPreviousFrame() { return frames[previousImageIdx]; }

        vk::Extent2D extent;
        uint32_t imageCount, currentImageIdx, previousImageIdx;
        vk::raii::SwapchainKHR swapchainKHR;
        vk::raii::CommandPool commandPool;
        std::vector<Frame> frames;
    };
}
