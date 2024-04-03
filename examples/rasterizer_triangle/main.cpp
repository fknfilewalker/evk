import evk;
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

[[noreturn]] void exitWithError(const std::string_view error = "") {
    if (error.empty()) std::printf("%s\n", error.data());
    exit(EXIT_FAILURE);
}

constexpr struct { uint32_t width, height; } target{ 800u, 600u };
int main(int /*argc*/, char** /*argv*/)
{
	if (!glfwInit()) exitWithError("Failed to init GLFW");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No need to create a graphics context for Vulkan
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(target.width, target.height, "Vulkan Rasterizer", nullptr, nullptr);

    // Instance Setup
    std::vector iExtensions{ vk::KHRSurfaceExtensionName, vk::KHRGetSurfaceCapabilities2ExtensionName };
    if(evk::isWindows) iExtensions.emplace_back("VK_KHR_win32_surface");
	if(evk::isApple) {
        iExtensions.emplace_back("VK_EXT_metal_surface");
        iExtensions.emplace_back(vk::KHRPortabilityEnumerationExtensionName);
    }
    std::vector iLayers = { "VK_LAYER_LUNARG_monitor" };
    if constexpr (evk::isDebug) iLayers.emplace_back("VK_LAYER_KHRONOS_validation");
    if constexpr (evk::isApple) iLayers.emplace_back("VK_LAYER_KHRONOS_shader_object");
    vk::InstanceCreateFlags instanceFlags = {};
    if constexpr (evk::isApple) instanceFlags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;

    const auto& ctx = evk::context();
    evk::utils::remExtsOrLayersIfNotAvailable(iExtensions, ctx.enumerateInstanceExtensionProperties(), [](const char* e) { std::printf("Extension removed because not available: %s\n", e); });
    evk::utils::remExtsOrLayersIfNotAvailable(iLayers, ctx.enumerateInstanceLayerProperties(), [](const char* e) { std::printf("Layer removed because not available: %s\n", e); });
	evk::Instance instance { ctx, instanceFlags, vk::ApplicationInfo{ nullptr, 0, nullptr, 0, vk::ApiVersion12 }, iLayers, iExtensions };

    // Surface Setup
#ifdef _WIN32
    const vk::raii::SurfaceKHR surface{ instance, vk::Win32SurfaceCreateInfoKHR{ {}, GetModuleHandle(nullptr), glfwGetWin32Window(window) } };
#elif __APPLE__
    const vk::raii::SurfaceKHR surface{ instance, vk::MetalSurfaceCreateInfoEXT{ {}, glfwGetMetalLayer(window) } };
#endif

    // Device setup
    const vk::raii::PhysicalDevices physicalDevices{ instance };
    const vk::raii::PhysicalDevice& physicalDevice{ physicalDevices[0] };
    // * find queue
    const auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    const auto queueFamilyIndex = evk::utils::findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eGraphics);
    if (!queueFamilyIndex.has_value()) exitWithError("No queue family index found");
    if (!physicalDevice.getSurfaceSupportKHR(queueFamilyIndex.value(), surface)) exitWithError("Queue family does not support presentation");
    // * check extensions
    std::vector dExtensions{ vk::KHRSwapchainExtensionName, vk::EXTShaderObjectExtensionName, vk::KHRDynamicRenderingExtensionName, vk::KHRSynchronization2ExtensionName };
    if constexpr (evk::isApple) dExtensions.emplace_back("VK_KHR_portability_subset");

    if (!evk::utils::extensionsOrLayersAvailable(physicalDevice.enumerateDeviceExtensionProperties(), dExtensions, [](const char* e) { std::printf("Extension not available: %s\n", e); })) exitWithError();
    // * activate features
    vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{ true };
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
    auto buffer = std::make_unique<evk::Buffer>( device, verticesSize, vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible ); /* reBAR */
    void* p = buffer->memory.mapMemory(0, vk::WholeSize);
    std::memcpy(p, vertices.data(), verticesSize);
    buffer->memory.unmapMemory();

    // Shader object setup
    // https://github.com/KhronosGroup/Vulkan-Docs/blob/main/proposals/VK_EXT_shader_object.adoc
    constexpr vk::PushConstantRange pcRange{ vk::ShaderStageFlagBits::eVertex, 0, sizeof(uint64_t) };
    evk::ShaderObject shader{ device, {
    	{ vk::ShaderStageFlagBits::eVertex, vertexShaderSPV, "main"},
    	{vk::ShaderStageFlagBits::eFragment, fragmentShaderSPV, "main"}
    }, {pcRange}};

    // Swapchain setup
    const auto sCapabilities = device->physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    const auto sFormats = device->physicalDevice.getSurfaceFormatsKHR(*surface);
    const vk::SwapchainCreateInfoKHR swapchainCreateInfo{ {}, *surface, evk::utils::clampSwapchainImageCount(2u, sCapabilities),
                sFormats[0].format, sFormats[0].colorSpace, sCapabilities.currentExtent, 1u, vk::ImageUsageFlagBits::eColorAttachment };
    evk::Swapchain swapchain{ device, swapchainCreateInfo, queueFamilyIndex.value() };
    vk::ImageMemoryBarrier2 imageMemoryBarrier{};
    imageMemoryBarrier.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    vk::DependencyInfo dependencyInfo = vk::DependencyInfo{}.setImageMemoryBarriers(imageMemoryBarrier);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) continue;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, GLFW_TRUE);
        swapchain.acquireNextImage();
        const auto& cFrame = swapchain.getCurrentFrame();
        const auto& cb = cFrame.commandBuffer;

        imageMemoryBarrier.image = cFrame.image;
        imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
        imageMemoryBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        cb.pipelineBarrier2(dependencyInfo);

        vk::RenderingAttachmentInfo rAttachmentInfo{ *cFrame.imageView, vk::ImageLayout::eColorAttachmentOptimal };
        rAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 1.0f };
        rAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
        rAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
        cb.beginRendering({ {}, { {}, swapchain.extent }, 1, 0, 1, &rAttachmentInfo });
        {
            /* set render state for shader objects */
            cb.bindShadersEXT(shader.stages, shader.shaders);
            cb.pushConstants<uint64_t>(*shader.layout, vk::ShaderStageFlagBits::eVertex, 0, /* for bindless rendering */ buffer->deviceAddress);
            cb.setPrimitiveTopologyEXT(vk::PrimitiveTopology::eTriangleList);
            cb.setPolygonModeEXT(vk::PolygonMode::eFill);
            cb.setFrontFaceEXT(vk::FrontFace::eCounterClockwise);
            cb.setCullModeEXT(vk::CullModeFlagBits::eNone);
            cb.setColorWriteMaskEXT(0, vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB);
            cb.setSampleMaskEXT(vk::SampleCountFlagBits::e1, { 0xffffffff });
            cb.setRasterizationSamplesEXT(vk::SampleCountFlagBits::e1);
            cb.setViewportWithCountEXT({ { 0, 0, static_cast<float>(swapchain.extent.width), static_cast<float>(swapchain.extent.height) } });
            cb.setScissorWithCountEXT({ { { 0, 0 }, swapchain.extent } });
            cb.setVertexInputEXT({}, {});
            cb.setColorBlendEnableEXT(0, vk::False);
            cb.setDepthTestEnableEXT(vk::False);
            cb.setDepthWriteEnableEXT(vk::False);
            cb.setDepthBiasEnableEXT(vk::False);
            cb.setStencilTestEnableEXT(vk::False);
            cb.setRasterizerDiscardEnableEXT(vk::False);
            cb.setColorBlendEquationEXT(0, vk::ColorBlendEquationEXT{}.setSrcColorBlendFactor(vk::BlendFactor::eOne));
            cb.setAlphaToCoverageEnableEXT(vk::False);
            cb.setPrimitiveRestartEnableEXT(vk::False);
            cb.draw(3, 1, 0, 0);
        }
        cb.endRendering();

        imageMemoryBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        imageMemoryBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        cb.pipelineBarrier2(dependencyInfo);
        swapchain.submitImage(device->getQueue(queueFamilyIndex.value(), 0), { vk::PipelineStageFlagBits::eColorAttachmentOutput });
    }
    device->waitIdle();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
