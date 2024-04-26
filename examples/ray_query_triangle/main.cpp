#include <cstdio>
#include <cstdint>
#include <vector>
#include <array>
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
    if(error.empty()) std::printf("%s\n", error.data());
    exit(EXIT_FAILURE);
}

constexpr struct { uint32_t width, height; } target{ 800u, 600u };
int main(int /*argc*/, char** /*argv*/)
{
    if (!glfwInit()) exitWithError("Failed to init GLFW");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No need to create a graphics context for Vulkan
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(target.width, target.height, "Vulkan Ray Query", nullptr, nullptr);

    // Instance Setup
    std::vector iExtensions{ vk::KHRSurfaceExtensionName, vk::KHRGetSurfaceCapabilities2ExtensionName };
    if (evk::isWindows) iExtensions.emplace_back("VK_KHR_win32_surface");
    if (evk::isApple) iExtensions.emplace_back("VK_EXT_metal_surface");

    std::vector iLayers = { "VK_LAYER_LUNARG_monitor" };
    if constexpr (evk::isDebug) iLayers.emplace_back("VK_LAYER_KHRONOS_validation");
    if constexpr (evk::isApple) iLayers.emplace_back("VK_LAYER_KHRONOS_shader_object");

    const auto& ctx = evk::context();
    evk::utils::remExtsOrLayersIfNotAvailable(iExtensions, ctx.enumerateInstanceExtensionProperties(), [](const char* e) { std::printf("Extension removed because not available: %s\n", e); });
    evk::utils::remExtsOrLayersIfNotAvailable(iLayers, ctx.enumerateInstanceLayerProperties(), [](const char* e) { std::printf("Layer removed because not available: %s\n", e); });
    evk::Instance instance{ ctx, {}, vk::ApplicationInfo{ nullptr, 0, nullptr, 0, vk::ApiVersion13 }, iLayers, iExtensions };

    // Surface Setup
#ifdef _WIN32
    const vk::Win32SurfaceCreateInfoKHR surfaceCreateInfo{ {}, nullptr, glfwGetWin32Window(window) };
#elif __APPLE__
    const vk::MetalSurfaceCreateInfoEXT surfaceCreateInfo{ {}, glfwGetCocoaWindow(window) };
#endif
    vk::raii::SurfaceKHR surface{ instance, surfaceCreateInfo };

    // Device setup
    const vk::raii::PhysicalDevices physicalDevices{ instance };
    const vk::raii::PhysicalDevice& physicalDevice{ physicalDevices[0] };
    // * find queue
    const auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    const auto queueFamilyIndex = evk::utils::findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eCompute);
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
    auto stcb = commandPool.allocateCommandBuffer();
    stcb.begin(vk::CommandBufferBeginInfo{});
    auto triangle = evk::rt::TriangleGeometry{}.setVertices(buffer->deviceAddress, vk::Format::eR32G32B32Sfloat, 3);
    evk::rt::BottomLevelAccelerationStructure blas{ device, stcb, { { triangle, {} } } };
    constexpr auto barrier = vk::MemoryBarrier2{
        vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR, vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
        vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR, vk::AccessFlagBits2::eAccelerationStructureWriteKHR
    };
    stcb.pipelineBarrier2({ {}, barrier });
    auto instances = evk::rt::AsInstanceGeometry{}.setAccelerationStructureReference(blas.deviceAddress).setMask(0xFF).setTransform(evk::rt::identityMatrix);
    auto tlas = evk::rt::TopLevelAccelerationStructure{ device, stcb, { instances } };
    stcb.end();
    device->getQueue(queueFamilyIndex.value(), 0).submitAndWaitIdle(vk::SubmitInfo{ {}, {}, *stcb }, nullptr);

    // Swapchain setup
    const auto sCapabilities = device->physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    const auto sFormats = device->physicalDevice.getSurfaceFormatsKHR(*surface);

    std::optional<vk::SurfaceFormatKHR> sFormat;
    for (size_t i = 0; i < sFormats.size() && !sFormat.has_value(); ++i) {
        if (device->imageFormatSupported(sFormats[i].format, vk::ImageType::e2D, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst))
            sFormat = sFormats[i];
	}
    if (!sFormat.has_value()) exitWithError("No suitable swapchain format found");

    const vk::SwapchainCreateInfoKHR swapchainCreateInfo{ {}, *surface, evk::utils::clampSwapchainImageCount(2u, sCapabilities),
                sFormat.value().format, sFormat.value().colorSpace, sCapabilities.currentExtent, 1u, vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst };
    evk::Swapchain swapchain{ device, swapchainCreateInfo, queueFamilyIndex.value() };
    vk::ImageMemoryBarrier2 imageMemoryBarrier{};
    imageMemoryBarrier.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    vk::DependencyInfo dependencyInfo = vk::DependencyInfo{}.setImageMemoryBarriers(imageMemoryBarrier);

    // Descriptor set setup
    std::vector<evk::DescriptorSet> descriptorSets;
    descriptorSets.reserve(swapchain.frames.size());
    for (const auto& frame : swapchain.frames) {
        descriptorSets.emplace_back(device, evk::DescriptorSet::Bindings{
            { { 0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eAll }, { vk::DescriptorBindingFlagBits::eVariableDescriptorCount } }
            });
        descriptorSets.back().setDescriptor(0, vk::DescriptorImageInfo{ {}, frame.imageView, vk::ImageLayout::eGeneral });
        descriptorSets.back().update();
    }

    // Shader object setup
    // https://github.com/KhronosGroup/Vulkan-Docs/blob/main/proposals/VK_EXT_shader_object.adoc
    const std::array workGroupSize = { device->subgroupProperties.subgroupSize / 4u, 4u }; // image usually width > height
    const std::array workGroupCount = {
        static_cast<uint32_t>(std::ceil(target.width / workGroupSize[0])),
        static_cast<uint32_t>(std::ceil(target.height / workGroupSize[1])) };
    evk::ShaderSpecialization shaderSpecialization{ {
        { 0u, 0u, sizeof(uint32_t) },
        { 1u, sizeof(uint32_t), sizeof(uint32_t) }
    }, &workGroupSize };

    constexpr vk::PushConstantRange pcRange{ vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint64_t) };
    evk::ShaderObject shader{ device, {
        { vk::ShaderStageFlagBits::eCompute, computeShaderSPV, "main" }
    }, { pcRange }, shaderSpecialization, { descriptorSets[0] } };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) continue;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, GLFW_TRUE);
        swapchain.acquireNextImage();
        const auto& cFrame = swapchain.getCurrentFrame();
        const auto& cb = cFrame.commandBuffer;

        imageMemoryBarrier.image = cFrame.image;
        imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
        imageMemoryBarrier.newLayout = vk::ImageLayout::eGeneral;
        cb.pipelineBarrier2(dependencyInfo);

        {
            cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, shader.layout, 0, { descriptorSets[swapchain.currentImageIdx] }, {});
            cb.bindShadersEXT(shader.stages, shader.shaders);
            cb.pushConstants<uint64_t>(*shader.layout, vk::ShaderStageFlagBits::eCompute, 0, tlas.deviceAddress);
            cb.dispatch(workGroupCount[0], workGroupCount[1], 1);
        }

        imageMemoryBarrier.oldLayout = vk::ImageLayout::eGeneral;
        imageMemoryBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        cb.pipelineBarrier2(dependencyInfo);
        swapchain.submitImage(device->getQueue(queueFamilyIndex.value(), 0));
    }
    device->waitIdle();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
