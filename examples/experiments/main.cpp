#include <cstdio>
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <string_view>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "shaders.h"

import evk;

[[noreturn]] void exitWithError(const std::string_view error = "") {
    if (error.empty()) std::printf("%s\n", error.data());
    exit(EXIT_FAILURE);
}

//https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_LUNARG_direct_driver_loading.html
//vk::LUNARGDirectDriverLoadingExtensionName
//vk::DirectDriverLoadingInfoLUNARG directDriverLoadingInfo{ {}, nullptr };
//vk::DirectDriverLoadingListLUNARG directDriverLoadingList{ vk::DirectDriverLoadingModeLUNARG::eInclusive, directDriverLoadingInfo };

int main(int /*argc*/, char** /*argv*/)
{
    // Instance Setup
    std::vector iExtensions{ vk::KHRSurfaceExtensionName, vk::EXTSurfaceMaintenance1ExtensionName, vk::KHRGetSurfaceCapabilities2ExtensionName };
    if (evk::isWindows) iExtensions.emplace_back("VK_KHR_win32_surface");
    if (evk::isApple) iExtensions.emplace_back("VK_EXT_metal_surface");

    std::vector iLayers = { "VK_LAYER_LUNARG_monitor" };
    if constexpr (evk::isDebug) iLayers.emplace_back("VK_LAYER_KHRONOS_validation");
    if constexpr (evk::isApple) iLayers.emplace_back("VK_LAYER_KHRONOS_shader_object");

    const auto& ctx = evk::context();
    evk::utils::remExtsOrLayersIfNotAvailable(iExtensions, ctx.enumerateInstanceExtensionProperties(), [](const char* e) { std::printf("Extension removed because not available: %s\n", e); });
    evk::utils::remExtsOrLayersIfNotAvailable(iLayers, ctx.enumerateInstanceLayerProperties(), [](const char* e) { std::printf("Layer removed because not available: %s\n", e); });
    evk::Instance instance{ ctx, {}, vk::ApplicationInfo{ nullptr, 0, nullptr, 0, vk::ApiVersion13 }, iLayers, iExtensions };

    // Device setup
    const vk::raii::PhysicalDevices physicalDevices{ instance };
    const vk::raii::PhysicalDevice& physicalDevice{ physicalDevices[0] };
    // * find queue
    const auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    const auto queueFamilyIndex = evk::utils::findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eCompute);
    if (!queueFamilyIndex.has_value()) exitWithError("No queue family index found");
    //if (!physicalDevice.getSurfaceSupportKHR(queueFamilyIndex.value(), *surface)) exitWithError("Queue family does not support presentation");
    // * check extensions
    std::vector dExtensions{ vk::EXTMutableDescriptorTypeExtensionName };
    if constexpr (evk::isApple) dExtensions.emplace_back("VK_KHR_portability_subset");

    if (!evk::utils::extensionsOrLayersAvailable(physicalDevice.enumerateDeviceExtensionProperties(), dExtensions, [](const char* e) { std::printf("Extension not available: %s\n", e); })) exitWithError();
    // * activate features
    //vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceRayTracingPipelineFeaturesKHR> featuresChain;
    vk::PhysicalDeviceMutableDescriptorTypeFeaturesEXT mutableDescriptorTypeFeatures{ true };
    vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2{ {}, &mutableDescriptorTypeFeatures };
    physicalDeviceFeatures2.features.shaderInt64 = true;
    // * create device
    auto device = evk::make_shared<evk::Device>(physicalDevice, dExtensions, evk::Device::Queues{ {queueFamilyIndex.value(), 1} }, &physicalDeviceFeatures2);

    std::vector types = { vk::DescriptorType::eSampledImage, vk::DescriptorType::eSampler, vk::DescriptorType::eStorageImage };
    vk::MutableDescriptorTypeListEXT mutate { types };
    vk::MutableDescriptorTypeCreateInfoEXT mutableDescriptorTypeCreateInfo{ mutate };
    vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding{ 0, vk::DescriptorType::eMutableEXT, 1, vk::ShaderStageFlagBits::eCompute };
    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.setPNext(&mutableDescriptorTypeCreateInfo);
    descriptorSetLayoutCreateInfo.setBindings(descriptorSetLayoutBinding);

    auto support = device->getDescriptorSetLayoutSupport(descriptorSetLayoutCreateInfo);

    /*evk::DescriptorSetLayout descriptorSetLayout{ device, {
        { { 0, vk::DescriptorType::eMutableEXT, 1, vk::ShaderStageFlagBits::eCompute }, vk::DescriptorBindingFlagBits::eVariableDescriptorCount }
    } };*/
    return 0;
}
