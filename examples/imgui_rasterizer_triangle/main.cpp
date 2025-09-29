#include <cstdio>
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <string_view>
#include <SDL3/SDL.h>
#include "shaders.h"
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>

import evk;
import evk.imgui;

[[noreturn]] void exitWithError(const std::string_view error = "") {
    if (error.empty()) std::printf("%s\n", error.data());
    exit(EXIT_FAILURE);
}

constexpr struct { uint32_t width, height; } target{ 800u, 600u };
int main(int /*argc*/, char** /*argv*/)
{
    if (!SDL_Init(0)) exitWithError("Failed to init SDL");
    SDL_Window* window = SDL_CreateWindow("Vulkan Rasterizer", target.width, target.height, SDL_WINDOW_RESIZABLE);

    // Instance Setup
    std::vector iExtensions{ vk::KHRSurfaceExtensionName, vk::EXTSurfaceMaintenance1ExtensionName, vk::KHRGetSurfaceCapabilities2ExtensionName };
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
    auto instance = evk::Instance::shared(ctx, instanceFlags, vk::ApplicationInfo{ nullptr, 0, nullptr, 0, vk::ApiVersion12 }, iLayers, iExtensions);

    // Surface Setup
    vk::raii::SurfaceKHR surface{ nullptr };
#ifdef VK_USE_PLATFORM_WIN32_KHR
    surface = vk::raii::SurfaceKHR{ instance, vk::Win32SurfaceCreateInfoKHR{ {}, nullptr, static_cast<evk::win::HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr)) } };
#elif VK_USE_PLATFORM_XLIB_KHR
#elif VK_USE_PLATFORM_WAYLAND_KHR
#elif VK_USE_PLATFORM_METAL_EXT
    surface = vk::raii::SurfaceKHR{ instance, vk::MetalSurfaceCreateInfoEXT{ {}, SDL_Metal_GetLayer(SDL_Metal_CreateView(window)) } };
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
    std::vector dExtensions{ vk::KHRSwapchainExtensionName, vk::EXTShaderObjectExtensionName, vk::KHRDynamicRenderingExtensionName, vk::KHRSynchronization2ExtensionName,
    vk::EXTHostImageCopyExtensionName, vk::KHRFormatFeatureFlags2ExtensionName, vk::KHRCopyCommands2ExtensionName, vk::EXTSwapchainMaintenance1ExtensionName };
    if constexpr (evk::isApple) dExtensions.emplace_back("VK_KHR_portability_subset");

    if (!evk::utils::extensionsOrLayersAvailable(physicalDevice.enumerateDeviceExtensionProperties(), dExtensions, [](const char* e) { std::printf("Extension not available: %s\n", e); })) exitWithError();
    // * activate features
    auto vulkan11Features = vk::PhysicalDeviceVulkan11Features{}
        .setVariablePointers(true).setVariablePointersStorageBuffer(true);
    auto vulkan12Features = vk::PhysicalDeviceVulkan12Features{}
        .setShaderInt8(true).setBufferDeviceAddress(true)
        .setDescriptorBindingVariableDescriptorCount(true)
        .setDescriptorBindingPartiallyBound(true).setPNext(&vulkan11Features);

    vk::PhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{ true, &vulkan12Features };
    vk::PhysicalDeviceHostImageCopyFeaturesEXT hostImageCopyFeatures{ true, &shaderObjectFeatures };
    vk::PhysicalDeviceSynchronization2Features synchronization2Features{ true, &hostImageCopyFeatures };
    vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchainMaintenance{ true, &synchronization2Features };
    vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{ true, &swapchainMaintenance };
    vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2{ {}, &dynamicRenderingFeatures };
    physicalDeviceFeatures2.features.shaderInt64 = true;
    // * create device
    auto device = evk::make_shared<evk::Device>(instance, physicalDevice, dExtensions, evk::Device::Queues{ {queueFamilyIndex.value(), 1} }, &physicalDeviceFeatures2);

    // Vertex buffer setup (triangle is upside down on purpose)
    const std::vector vertices = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    const size_t verticesSize = vertices.size() * sizeof(float);
    auto buffer = std::make_unique<evk::Buffer>(device, verticesSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible); /* reBAR */
    void* p = buffer->memory.mapMemory(0, vk::WholeSize);
    std::memcpy(p, vertices.data(), verticesSize);
    buffer->memory.unmapMemory();

    // Shader object setup
    // https://github.com/KhronosGroup/Vulkan-Docs/blob/main/proposals/VK_EXT_shader_object.adoc
    constexpr vk::PushConstantRange pcRange{ vk::ShaderStageFlagBits::eVertex, 0, sizeof(uint64_t) };
    evk::ShaderObject shader{ device, {
        { vk::ShaderStageFlagBits::eVertex, vertexShaderSPV, "main"},
        {vk::ShaderStageFlagBits::eFragment, fragmentShaderSPV, "main"}
    }, {pcRange} };

    // Swapchain setup
    const auto sCapabilities = device->physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    const auto sFormats = device->physicalDevice.getSurfaceFormatsKHR(*surface);
    const vk::SwapchainCreateInfoKHR swapchainCreateInfo{ {}, *surface, evk::utils::clampSwapchainImageCount(2u, sCapabilities),
                sFormats[0].format, sFormats[0].colorSpace, sCapabilities.currentExtent, 1u, vk::ImageUsageFlagBits::eColorAttachment };
    evk::Swapchain swapchain{ device, swapchainCreateInfo, queueFamilyIndex.value() };
    vk::ImageMemoryBarrier2 imageMemoryBarrier{};
    imageMemoryBarrier.setSubresourceRange({ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
    vk::DependencyInfo dependencyInfo = vk::DependencyInfo{}.setImageMemoryBarriers(imageMemoryBarrier);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    constexpr float scaleFactor = 1.0f;
    ImGui::GetStyle().ScaleAllSizes(scaleFactor);
    ImGui::StyleColorsDark();

    evk::ImGuiBackend imguiBackend{ device, swapchain.imageCount() };
    imguiBackend.setFont();
    ImGui_ImplSDL3_InitForVulkan(window);

    bool running = true, minimized = false;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) { running = false; break; }
            if (event.type == SDL_EVENT_WINDOW_MINIMIZED) { minimized = true; break; }
            if (event.type == SDL_EVENT_WINDOW_RESTORED) { minimized = false; break; }
        }
        if (minimized) continue;

        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Another Window");
        ImGui::Text("Hello from another window!");
        ImGui::End();
        ImGui::Render();

        swapchain.acquireNextImage();
        const auto& cFrame = swapchain.getCurrentFrame();
        const auto& cb = cFrame.commandBuffer;

        imageMemoryBarrier.setImage(swapchain.getCurrentImage())
            .setOldLayout(vk::ImageLayout::eUndefined).setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eAllCommands).setSrcAccessMask({})
            .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput).setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite);
        cb.pipelineBarrier2(dependencyInfo);

        vk::RenderingAttachmentInfo rAttachmentInfo{ *swapchain.getCurrentImageView(), vk::ImageLayout::eColorAttachmentOptimal };
        rAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };
        rAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
        rAttachmentInfo.storeOp = vk::AttachmentStoreOp::eStore;
        cb.beginRendering({ {}, { {}, swapchain.extent()}, 1, 0, 1, &rAttachmentInfo});
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
            cb.setViewportWithCountEXT({ { 0, 0, static_cast<float>(swapchain.extent().width), static_cast<float>(swapchain.extent().height)}});
            cb.setScissorWithCountEXT({ { { 0, 0 }, swapchain.extent()}});
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

            imguiBackend.render(cb, swapchain.currentImageIdx);
        }
        cb.endRendering();

        imageMemoryBarrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal).setNewLayout(vk::ImageLayout::ePresentSrcKHR)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput).setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eNone).setDstAccessMask(vk::AccessFlagBits2::eNone);
        cb.pipelineBarrier2(dependencyInfo);
        swapchain.submitImage(device->getQueue(queueFamilyIndex.value(), 0), vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    }
    device->waitIdle();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
