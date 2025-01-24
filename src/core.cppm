module;
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <string_view>
#include <deque>
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
    struct Instance : vk::raii::Instance, Shareable<Instance>
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

    struct InstanceLink { evk::SharedPtr<Instance> _instance; };
    struct Device : InstanceLink, vk::raii::Device, Shareable<Device>
    {
        using QueueFamily = uint32_t;
        using QueueCount = uint32_t;
        using Queues = std::unordered_map<QueueFamily, QueueCount>;

        EVK_API Device() : vk::raii::Device{ nullptr }, physicalDevice{ nullptr }{}
        EVK_API Device(
            const evk::SharedPtr<Instance>& instance,
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
        ) const;

        EVK_API const Queue& getQueue(
            const std::pair<QueueFamily, QueueCount>& queueIndices
        ) const;

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
    struct Resource { evk::SharedPtr<Device> dev; };

    struct CommandPool : Resource
    {
        EVK_API CommandPool(
            const evk::SharedPtr<Device>& device,
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
    struct Buffer : Resource, Shareable<Buffer>
    {
        EVK_API Buffer();
        EVK_API Buffer(
            const evk::SharedPtr<Device>& device,
            vk::DeviceSize size,
            vk::BufferUsageFlags usageFlags,
            vk::MemoryPropertyFlags memoryPropertyFlags
        );
        EVK_API void resize(const vk::DeviceSize& s);
        vk::raii::Buffer buffer;
        vk::raii::DeviceMemory memory;
        vk::DeviceAddress deviceAddress;
        vk::DeviceSize size;

        vk::BufferUsageFlags _usageFlags;
        vk::MemoryPropertyFlags _memoryPropertyFlags;
    };

    struct ImageMemoryBarrier2 : vk::ImageMemoryBarrier2 {

        constexpr ImageMemoryBarrier2& reset_src()
        {
            this->setSrcStageMask(vk::PipelineStageFlagBits2::eAllCommands).setSrcAccessMask({});
            return *this;
        }
        constexpr ImageMemoryBarrier2& reset_dst()
        {
            this->setDstStageMask(vk::PipelineStageFlagBits2::eAllCommands).setDstAccessMask({});
            return *this;
        }
        constexpr ImageMemoryBarrier2& swap_src_dst()
        {
            srcStageMask = dstStageMask;
            srcAccessMask = dstAccessMask;
            oldLayout = newLayout;
            dstStageMask = {};
            dstAccessMask = {};
            newLayout = {};
            return *this;
        }
        ImageMemoryBarrier2& pipelineBarrier(const vk::raii::CommandBuffer& cb) {
            cb.pipelineBarrier2(vk::DependencyInfo{}.setImageMemoryBarriers(*this));
            return *this;
        }
        [[nodiscard]] bool is_layout(vk::ImageLayout l) const { return oldLayout == l; }
        [[nodiscard]] bool is_not_layout(vk::ImageLayout l) const { return oldLayout != l; }
    };

    struct Image : Resource
    {
        EVK_API Image() : Resource{ nullptr }, image{ nullptr }, imageView{ nullptr }, memory{ nullptr }, format{ vk::Format::eUndefined }, _tiling{} {}
        EVK_API Image(
            const evk::SharedPtr<Device>& device,
            vk::Extent3D extent,
            vk::Format format,
            vk::ImageTiling tiling,
            vk::ImageUsageFlags usageFlags,
            vk::MemoryPropertyFlags memoryPropertyFlags
        );

        EVK_API void resize(vk::Extent3D ex);
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
        evk::ImageMemoryBarrier2 barrier;

        vk::ImageTiling _tiling;
        vk::ImageUsageFlags _usageFlags;
        vk::MemoryPropertyFlags _memoryPropertyFlags;
    };

    struct MutableDescriptorSetLayout : Resource
    {
        EVK_API MutableDescriptorSetLayout() : Resource{ nullptr }, layout{ nullptr }, descriptorCount{ 0 } {}
        EVK_API MutableDescriptorSetLayout(
            const evk::SharedPtr<Device>& device,
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
            const evk::SharedPtr<Device>& device,
            const MutableDescriptorSetLayout& layout,
            const vk::DescriptorPoolCreateFlags& poolFlags = {},
			const void* pNext = nullptr
        ) : Resource{ device }, pool{ nullptr }, set{ nullptr }
        {
			// pool
	        const vk::DescriptorPoolSize poolSize { vk::DescriptorType::eMutableEXT, layout.descriptorCount };
            const vk::DescriptorPoolCreateInfo descPoolInfo{ poolFlags | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, poolSize };
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
            const evk::SharedPtr<Device>& device,
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
        using Descriptor = std::variant<vk::DescriptorImageInfo, vk::DescriptorBufferInfo, vk::BufferView, vk::AccelerationStructureKHR>;
        using Descriptors = std::variant<std::vector<vk::DescriptorImageInfo>, std::vector<vk::DescriptorBufferInfo>, std::vector<vk::BufferView>, std::vector<vk::AccelerationStructureKHR>>;
        EVK_API DescriptorSet() : Resource{ nullptr }, pool{ nullptr }, set{ nullptr } {}
        EVK_API DescriptorSet(
            const evk::SharedPtr<Device>& device,
            const DescriptorSetLayout& layout
        ) : Resource{ device }, _bindings{ layout._bindings }, pool{ nullptr }, set{ nullptr }
        {
            // pool
            std::vector<vk::DescriptorPoolSize> poolSizes(_bindings.size());
            for (size_t i = 0; i < _bindings.size(); i++) poolSizes[i].setType(_bindings[i].first.descriptorType).setDescriptorCount(_bindings[i].first.descriptorCount);
            const vk::DescriptorPoolCreateInfo descPoolInfo{ vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, poolSizes };
            pool = vk::raii::DescriptorPool{ *dev, descPoolInfo };

            // descriptor set
            const vk::DescriptorSetVariableDescriptorCountAllocateInfo varDescCountAllocInfo = { 1, &_bindings.back().first.descriptorCount };
            const vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo{ *pool, *layout.layout, &varDescCountAllocInfo };
            set = dev->allocateDescriptorSets(descriptorSetAllocateInfo)[0].release();

            // make room for descriptors
            for (size_t i = 0; i < _bindings.size(); ++i)
            {
                switch (_bindings[i].first.descriptorType)
                {
                case vk::DescriptorType::eSampler:
                case vk::DescriptorType::eCombinedImageSampler:
                case vk::DescriptorType::eSampledImage:
                case vk::DescriptorType::eStorageImage:
                    _descriptors.emplace(_bindings[i].first.binding, std::vector<vk::DescriptorImageInfo>{ _bindings[i].first.descriptorCount });
                    break;
                case vk::DescriptorType::eUniformTexelBuffer:
                case vk::DescriptorType::eStorageTexelBuffer:
                    _descriptors.emplace(_bindings[i].first.binding, std::vector<vk::BufferView>{ _bindings[i].first.descriptorCount });
                    break;
                case vk::DescriptorType::eUniformBuffer:
                case vk::DescriptorType::eStorageBuffer:
                case vk::DescriptorType::eUniformBufferDynamic:
                case vk::DescriptorType::eStorageBufferDynamic:
                    _descriptors.emplace(_bindings[i].first.binding, std::vector<vk::DescriptorBufferInfo>{ _bindings[i].first.descriptorCount });
                    break;
                case vk::DescriptorType::eAccelerationStructureKHR:
                    _descriptors.emplace(_bindings[i].first.binding, std::vector<vk::AccelerationStructureKHR>{ _bindings[i].first.descriptorCount });
                    break;
                default: 
                    throw std::runtime_error("Descriptor type not supported");
                }
            }
        }

        EVK_API DescriptorSet(
            const evk::SharedPtr<Device>& device,
            const DescriptorSetLayout::Bindings& bindings
        ) : DescriptorSet{ device, DescriptorSetLayout{ device, bindings } } {}

        EVK_API void setDescriptor(uint32_t binding, const Descriptor& data, uint32_t index = 0);

        EVK_API void update();

        //EVK_API operator const vk::DescriptorSetLayout& () const { return *layout; }
        EVK_API operator const vk::DescriptorSet& () const { return set; }

        DescriptorSetLayout::Bindings _bindings;
        std::map<uint32_t, Descriptors> _descriptors;
        //vk::raii::DescriptorSetLayout layout;
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

    // stage, module, entryPoint
    using ShaderModule = std::tuple<const vk::ShaderStageFlagBits, std::reference_wrapper<const vk::raii::ShaderModule>, std::string_view>;
    using ShaderModules = std::vector<ShaderModule>;

    // stage, spv, entryPoint
    using ShaderStage = std::tuple<const vk::ShaderStageFlagBits, const std::reference_wrapper<const std::vector<uint32_t>>, std::string_view>;

    struct ShaderObject : Resource
    {
        EVK_API ShaderObject() : Resource{ nullptr }, layout{ nullptr } {}
        EVK_API ShaderObject(
            const evk::SharedPtr<Device>& device,
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
            EVK_API Frame(const vk::raii::Device& device, const vk::raii::CommandPool& commandPool) :
                presentFinishFence{ device, vk::FenceCreateInfo{} }, imageAvailableSemaphore{ device, vk::SemaphoreCreateInfo{} }, renderFinishedSemaphore{ device, vk::SemaphoreCreateInfo{} },
                commandBuffer{ std::move(vk::raii::CommandBuffers{ device, { *commandPool, vk::CommandBufferLevel::ePrimary, 1 } }[0]) }
            {}
            vk::raii::Fence presentFinishFence;
            vk::raii::Semaphore imageAvailableSemaphore, renderFinishedSemaphore;
            vk::raii::CommandBuffer commandBuffer;
        };

        EVK_API Swapchain(const evk::SharedPtr<Device>& device, const vk::SwapchainCreateInfoKHR& createInfo, const uint32_t queueFamilyIndex) : Resource{ device }, currentImageIdx{ 0 }, previousImageIdx{ 0 },
            swapchain{ nullptr }, commandPool{ *dev, { vk::CommandPoolCreateFlagBits::eTransient, queueFamilyIndex } }
        {
            swapchainCreateInfo = createInfo;
            currentImageIdx = swapchainCreateInfo.minImageCount - 1u; // just for init
            createSwapchain();
        }

        EVK_API ~Swapchain()
        {
            while(frames.size()) {
                for (auto it = frames.begin(); it != frames.end(); (it->presentFinishFence.getStatus() == vk::Result::eSuccess) ? it = frames.erase(it) : ++it) {}
            }
        }

        EVK_API void createSwapchain() {
            const auto surfaceCapabilities = dev->physicalDevice.getSurfaceCapabilitiesKHR(swapchainCreateInfo.surface);
            swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
            swapchainCreateInfo.oldSwapchain = *swapchain;
            swapchain = vk::raii::SwapchainKHR{ *dev, swapchainCreateInfo };
            images = swapchain.getImages();
            views.clear(); for (const auto& image : images) views.emplace_back(nullptr);
        }

        EVK_API Frame& acquireNewFrame() {
            for (auto it = frames.begin(); it != frames.end(); (it->presentFinishFence.getStatus() == vk::Result::eSuccess) ? it = frames.erase(it) : ++it) {}
            frames.emplace_back(*dev, commandPool); // create a new frame
            return frames.back();
        }

        EVK_API void acquireNextImage() {
            auto& frame = acquireNewFrame();
            try {
                currentImageIdx = swapchain.acquireNextImage(UINT64_MAX, *frame.imageAvailableSemaphore).second;
            }
            catch (const vk::OutOfDateKHRError&) { createSwapchain(); acquireNextImage(); return; } // unix
            /* create image view after image is acquired because of vk::SwapchainCreateFlagBitsKHR::eDeferredMemoryAllocationEXT */
            if (not *views[currentImageIdx]) {
                views[currentImageIdx] = vk::raii::ImageView{ *dev, vk::ImageViewCreateInfo{ {}, images[currentImageIdx], vk::ImageViewType::e2D,
                    swapchainCreateInfo.imageFormat, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } } };
            }
            frame.commandBuffer.begin({});
        }

        EVK_API void submitImage(const vk::raii::Queue& presentQueue, const vk::PipelineStageFlags2 waitDstStageMask = vk::PipelineStageFlagBits2::eNone) {
            auto& frame = frames.back();
            frame.commandBuffer.end();

            vk::SemaphoreSubmitInfo wait = { *frame.imageAvailableSemaphore, {}, waitDstStageMask };
            vk::CommandBufferSubmitInfo present = { *frame.commandBuffer };
            vk::SemaphoreSubmitInfo signal = { *frame.renderFinishedSemaphore, {}, vk::PipelineStageFlagBits2::eAllCommands };
            presentQueue.submit2(vk::SubmitInfo2{ {}, wait, present, signal });
            vk::SwapchainPresentFenceInfoEXT presentFenceInfo{ *frame.presentFinishFence };
            try { auto _ = presentQueue.presentKHR({ *frame.renderFinishedSemaphore, *swapchain, currentImageIdx, {}, &presentFenceInfo }); }
            catch (const vk::OutOfDateKHRError&) { presentQueue.waitIdle(); frames.clear(); createSwapchain(); } // win32
        }

        EVK_API Frame& getCurrentFrame() { return frames.back(); }
        EVK_API vk::Image& getCurrentImage() { return images[currentImageIdx]; }
        EVK_API vk::raii::ImageView& getCurrentImageView() { return views[currentImageIdx]; }
        EVK_API [[nodiscard]] const vk::Extent2D& extent() const { return swapchainCreateInfo.imageExtent; }
        EVK_API [[nodiscard]] uint32_t imageCount() const { return swapchainCreateInfo.minImageCount; }

        vk::SwapchainCreateInfoKHR swapchainCreateInfo;
        uint32_t currentImageIdx, previousImageIdx;
        vk::raii::SwapchainKHR swapchain;
        std::vector<vk::Image> images;
        std::vector<vk::raii::ImageView> views;
        vk::raii::CommandPool commandPool;
        std::deque<Frame> frames;
    };
}
