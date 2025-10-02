#include <cstdio>
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <string_view>
#include <fstream>
#include <filesystem>

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
    auto instance = evk::Instance::shared(ctx, vk::InstanceCreateFlags{}, vk::ApplicationInfo{ nullptr, 0, nullptr, 0, vk::ApiVersion13 }, iLayers, iExtensions);

    // Device setup
    const vk::raii::PhysicalDevices physicalDevices{ instance };
    const vk::raii::PhysicalDevice& physicalDevice{ physicalDevices[0] };
	std::printf("Device: %s\n", physicalDevice.getProperties().deviceName.data());
    // * find queue
    const auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    const auto queueFamilyIndex = evk::utils::findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eCompute);
    if (!queueFamilyIndex.has_value()) exitWithError("No queue family index found");
    //if (!physicalDevice.getSurfaceSupportKHR(queueFamilyIndex.value(), *surface)) exitWithError("Queue family does not support presentation");
    // * check extensions
    std::vector dExtensions{ vk::EXTMutableDescriptorTypeExtensionName, vk::EXTShaderObjectExtensionName };
    if constexpr (evk::isApple) dExtensions.emplace_back("VK_KHR_portability_subset");

    if (!evk::utils::extensionsOrLayersAvailable(physicalDevice.enumerateDeviceExtensionProperties(), dExtensions, [](const char* e) { std::printf("Extension not available: %s\n", e); })) exitWithError();
    // * activate features
    //vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceRayTracingPipelineFeaturesKHR> featuresChain;
    vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{ true };
    vk::PhysicalDeviceMutableDescriptorTypeFeaturesEXT mutableDescriptorTypeFeatures{ true, &bufferDeviceAddressFeatures };
    vk::PhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{ true, &mutableDescriptorTypeFeatures };
    vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2{ {}, &shaderObjectFeatures };
    physicalDeviceFeatures2.features.shaderInt64 = true;
    // * create device
    auto device = evk::make_shared<evk::Device>(instance, physicalDevice, dExtensions, evk::Device::Queues{ {queueFamilyIndex.value(), 1} }, &physicalDeviceFeatures2);

    std::vector types = { vk::DescriptorType::eSampledImage, vk::DescriptorType::eSampler, vk::DescriptorType::eStorageImage };
    vk::MutableDescriptorTypeListEXT mutate { types };
    vk::MutableDescriptorTypeCreateInfoEXT mutableDescriptorTypeCreateInfo{ mutate };
    vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding{ 0, vk::DescriptorType::eMutableEXT, 1, vk::ShaderStageFlagBits::eCompute };
    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.setPNext(&mutableDescriptorTypeCreateInfo);
    descriptorSetLayoutCreateInfo.setBindings(descriptorSetLayoutBinding);

    auto support = device->getDescriptorSetLayoutSupport(descriptorSetLayoutCreateInfo);

    // load spv
	std::filesystem::path p = std::filesystem::current_path();
	std::printf("Current path is %s\n", p.string().c_str());
	std::vector<uint32_t> spv;
	std::fstream file("examples/experiments/shader.spv", std::ios::in | std::ios::binary);
	if (!file.is_open()) exitWithError("Could not open file");
	file.seekg(0, std::ios::end);
    auto bytes = file.tellg();
	spv.resize(bytes /4u);
	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char*>(spv.data()), bytes);
	file.close();

	// freeze happens here
    constexpr vk::PushConstantRange pcRange{ vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, 40 };
	auto shader = evk::ShaderObject{ device, {
        { vk::ShaderStageFlagBits::eVertex, spv, "vertexMain"},
        {vk::ShaderStageFlagBits::eFragment, spv, "fragmentMain"}
    }, {pcRange} };

    return 0;
}
