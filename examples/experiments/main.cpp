#include <cstdio>
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <string_view>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#elif __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "shaders.h"

import evk;

[[noreturn]] void exitWithError(const std::string_view error = "") {
    if (error.empty()) std::printf("%s\n", error.data());
    exit(EXIT_FAILURE);
}

//https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_LUNARG_direct_driver_loading.html


constexpr uint32_t window_width = 800;
constexpr uint32_t window_height = 600;
int main(int /*argc*/, char** /*argv*/)
{
    if (!glfwInit()) exitWithError("Failed to init GLFW");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No need to create a graphics context for Vulkan
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "Vulkan Triangle Modern", nullptr, nullptr);

    //vk::LUNARGDirectDriverLoadingExtensionName
    //vk::DirectDriverLoadingInfoLUNARG directDriverLoadingInfo{ {}, nullptr };
    //vk::DirectDriverLoadingListLUNARG directDriverLoadingList{ vk::DirectDriverLoadingModeLUNARG::eInclusive, directDriverLoadingInfo };

    // Instance Setup
    std::vector iExtensions{ vk::KHRGetSurfaceCapabilities2ExtensionName };
    uint32_t glfwInstanceExtensionCount;
    const char** glfwInstanceExtensionNames = glfwGetRequiredInstanceExtensions(&glfwInstanceExtensionCount);
    iExtensions.reserve(static_cast<size_t>(glfwInstanceExtensionCount) + 1u);
    for (uint32_t i = 0; i < glfwInstanceExtensionCount; ++i) iExtensions.emplace_back(glfwInstanceExtensionNames[i]);

    std::vector iLayers = { "VK_LAYER_LUNARG_monitor" };
    if constexpr (evk::isDebug) iLayers.emplace_back("VK_LAYER_KHRONOS_validation");
    if constexpr (evk::isApple) iLayers.emplace_back("VK_LAYER_KHRONOS_shader_object");

    const auto& ctx = evk::context();
    evk::utils::remExtsOrLayersIfNotAvailable(iExtensions, ctx.enumerateInstanceExtensionProperties(), [](const char* e) { std::printf("Extension removed because not available: %s\n", e); });
    evk::utils::remExtsOrLayersIfNotAvailable(iLayers, ctx.enumerateInstanceLayerProperties(), [](const char* e) { std::printf("Layer removed because not available: %s\n", e); });
    evk::Instance instance{ ctx, {}, vk::ApplicationInfo{ nullptr, 0, nullptr, 0, vk::ApiVersion13 }, iLayers, iExtensions };


    // Surface Setup
#ifdef VK_USE_PLATFORM_WIN32_KHR
    const vk::raii::SurfaceKHR surface{ instance, vk::Win32SurfaceCreateInfoKHR{ {}, GetModuleHandle(nullptr), glfwGetWin32Window(window) } };
#elif VK_USE_PLATFORM_METAL_EXT
    const vk::raii::SurfaceKHR surface{ instance, vk::MetalSurfaceCreateInfoEXT{ {}, glfwGetMetalLayer(window) } };
#endif

    // Device setup
    const vk::raii::PhysicalDevices physicalDevices{ instance };
    const vk::raii::PhysicalDevice& physicalDevice{ physicalDevices[0] };
    // * find queue
    const auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    const auto queueFamilyIndex = evk::utils::findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eGraphics);
    if (!queueFamilyIndex.has_value()) exitWithError("No queue family index found");
    if (!physicalDevice.getSurfaceSupportKHR(queueFamilyIndex.value(), *surface)) exitWithError("Queue family does not support presentation");
    // * check extensions
    std::vector dExtensions{ vk::KHRSwapchainExtensionName, vk::EXTShaderObjectExtensionName, vk::KHRDynamicRenderingExtensionName,
        vk::KHRRayQueryExtensionName, vk::KHRAccelerationStructureExtensionName, vk::KHRDeferredHostOperationsExtensionName,
        vk::KHRFormatFeatureFlags2ExtensionName, vk::KHRSynchronization2ExtensionName, vk::KHRMaintenance4ExtensionName };
    if constexpr (evk::isApple) dExtensions.emplace_back("VK_KHR_portability_subset");

    if (!evk::utils::extensionsOrLayersAvailable(physicalDevice.enumerateDeviceExtensionProperties(), dExtensions, [](const char* e) { std::printf("Extension not available: %s\n", e); })) exitWithError();
    // * activate features
    //vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceRayTracingPipelineFeaturesKHR> featuresChain;
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{ true };
    vk::PhysicalDeviceMaintenance4Features maintenance4Features{ true, &accelerationStructureFeatures };
    auto indexingFeatures = vk::PhysicalDeviceDescriptorIndexingFeatures{}.setDescriptorBindingVariableDescriptorCount(true).setDescriptorBindingPartiallyBound(true).setPNext(&maintenance4Features);
    vk::PhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{ true, &indexingFeatures };
    vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{ true, false, false, &rayQueryFeatures };
    vk::PhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{ true, &bufferDeviceAddressFeatures };
    vk::PhysicalDeviceSynchronization2Features synchronization2Features{ true, &shaderObjectFeatures };
    vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{ true, &synchronization2Features };
    vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2{ {}, &dynamicRenderingFeatures };
    physicalDeviceFeatures2.features.shaderInt64 = true;
    // * create device
    auto device = std::make_shared<evk::Device>(physicalDevice, dExtensions, evk::Device::Queues{ {queueFamilyIndex.value(), 1} }, &physicalDeviceFeatures2);

    // Vertex buffer setup (triangle is upside down on purpose)
    const std::vector vertices = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    const size_t verticesSize = vertices.size() * sizeof(float);
    auto buffer = std::make_unique<evk::Buffer>(device, verticesSize, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR, vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible); /* reBAR */
    void* p = buffer->memory.mapMemory(0, vk::WholeSize);
    std::memcpy(p, vertices.data(), verticesSize);
    buffer->memory.unmapMemory();

    // Acceleration structure setup
    evk::CommandPool commandPool{ device, queueFamilyIndex.value() };
    auto cb = commandPool.allocateCommandBuffer();
    cb.begin(vk::CommandBufferBeginInfo{});
    auto triangle = evk::rt::TriangleGeometry{}.setVertices(buffer->deviceAddress, vk::Format::eR32G32B32Sfloat, 3);
    evk::rt::BottomLevelAccelerationStructure blas{ device, cb, { { triangle, {} } } };

    constexpr auto barrier = vk::MemoryBarrier2{
        vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR, vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
        vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR, vk::AccessFlagBits2::eAccelerationStructureWriteKHR
    };
    cb.pipelineBarrier2({ {}, barrier });

    auto instances = evk::rt::AsInstanceGeometry{}.setAccelerationStructureReference(blas.deviceAddress).setMask(0xFF).setTransform(evk::rt::identityMatrix);
    auto tlas = evk::rt::TopLevelAccelerationStructure{ device, cb, { instances } };
    cb.end();
    device->getQueue(queueFamilyIndex.value(), 0).submitAndWaitIdle(vk::SubmitInfo{ {}, {}, *cb }, nullptr);


    //evk::rt::SBT sbt{ device, { 0 }, { 1u }, {} };
    //evk::rt::RayTracingPipeline rtp{ device, shadermodules, sbt, {}, shaderSpecialization };

    // Swapchain setup
    const auto sCapabilities = device->physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    const auto sFormats = device->physicalDevice.getSurfaceFormatsKHR(*surface);
    const vk::SwapchainCreateInfoKHR swapchainCreateInfo{ {}, *surface, evk::utils::clampSwapchainImageCount(2u, sCapabilities),
                sFormats[0].format, sFormats[0].colorSpace, sCapabilities.currentExtent, 1u, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst };
    evk::Swapchain swapchain{ device, swapchainCreateInfo, queueFamilyIndex.value() };
    vk::ImageMemoryBarrier2 imageMemoryBarrier{};
    imageMemoryBarrier.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    vk::DependencyInfo dependencyInfo = vk::DependencyInfo{}.setImageMemoryBarriers(imageMemoryBarrier);

    // Descriptor set setup
    std::vector<evk::DescriptorSet> descriptorSets;
    descriptorSets.reserve(swapchain.frames.size());
    for (const auto& frame : swapchain.frames) {
		descriptorSets.emplace_back(device, evk::DescriptorSet::Bindings{
            { { 0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eAll } }
		});
		descriptorSets.back().setDescriptor(0, vk::DescriptorImageInfo{ {}, frame.imageView, vk::ImageLayout::eGeneral });
		descriptorSets.back().update();
	}

    // Shader object setup
    // https://github.com/KhronosGroup/Vulkan-Docs/blob/main/proposals/VK_EXT_shader_object.adoc
    std::array<uint32_t, 3> work_group = { 1u, 1u, 1u };
    evk::ShaderSpecialization shaderSpecialization{
        {
            {0, 0, sizeof(uint32_t) },
            {1, sizeof(uint32_t), sizeof(uint32_t) },
            {2, sizeof(uint32_t), sizeof(uint32_t) }
        }, &work_group };

    constexpr vk::PushConstantRange pcRange{ vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint64_t) };
    evk::ShaderObject shader{ device, {
        { vk::ShaderStageFlagBits::eCompute, computeShaderSPV, "main" }
    }, { pcRange }, {}, { descriptorSets[0]}};

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) continue;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, GLFW_TRUE);
        swapchain.acquireNextImage();
        const auto& cFrame = swapchain.getCurrentFrame();
        const auto& cmdBuffer = cFrame.commandBuffer;

        imageMemoryBarrier.image = cFrame.image;
        imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
        imageMemoryBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        cmdBuffer.pipelineBarrier2(dependencyInfo);

        {
            cmdBuffer.clearColorImage(cFrame.image, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue{ std::array{ 0.0f, 0.0f, 0.0f, 1.0f } }, { { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } });
            cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, shader.layout, 0, { descriptorSets[swapchain.currentImageIdx] }, {});
            cmdBuffer.bindShadersEXT(shader.stages, shader.shaders);
            cmdBuffer.pushConstants<uint64_t>(*shader.layout, vk::ShaderStageFlagBits::eCompute, 0, tlas.deviceAddress);
            cmdBuffer.dispatch(window_width, window_height, 1);
        }

        imageMemoryBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        imageMemoryBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        cmdBuffer.pipelineBarrier2(dependencyInfo);
        swapchain.submitImage(device->getQueue(queueFamilyIndex.value(), 0));
    }
    device->waitIdle();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
