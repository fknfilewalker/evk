#include <cstdio>
#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <fstream>
#include "shaders.h"

import evk;

[[noreturn]] void exitWithError(const std::string_view error = "") {
    if(error.empty()) std::printf("%s\n", error.data());
    exit(EXIT_FAILURE);
}

constexpr struct { uint32_t width, height; } target{ 800u, 600u };
int main(int /*argc*/, char** /*argv*/)
{
    // Instance Setup
    std::vector<const char*> iExtensions {};
    if(evk::isApple) iExtensions.emplace_back(vk::KHRPortabilityEnumerationExtensionName);

    std::vector<const char*> iLayers {};
    if constexpr (evk::isDebug) iLayers.emplace_back("VK_LAYER_KHRONOS_validation");
    if constexpr (evk::isApple) iLayers.emplace_back("VK_LAYER_KHRONOS_shader_object");

    const auto& ctx = evk::context();
    evk::utils::remExtsOrLayersIfNotAvailable(iExtensions, ctx.enumerateInstanceExtensionProperties(), [](const char* e) { std::printf("Extension removed because not available: %s\n", e); });
    evk::utils::remExtsOrLayersIfNotAvailable(iLayers, ctx.enumerateInstanceLayerProperties(), [](const char* e) { std::printf("Layer removed because not available: %s\n", e); });
    
    vk::InstanceCreateFlags instanceFlags = {};
    if constexpr (evk::isApple) instanceFlags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
    evk::Instance instance{ ctx, instanceFlags, vk::ApplicationInfo{ nullptr, 0, nullptr, 0, vk::ApiVersion13 }, iLayers, iExtensions };

    // Device setup
    const vk::raii::PhysicalDevices physicalDevices{ instance };
    const vk::raii::PhysicalDevice& physicalDevice{ physicalDevices[0] };
    // * find queue
    const auto queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    const auto queueFamilyIndex = evk::utils::findQueueFamilyIndex(queueFamilyProperties, vk::QueueFlagBits::eCompute);
    if (!queueFamilyIndex.has_value()) exitWithError("No queue family index found");
    // * check extensions
    std::vector dExtensions{ vk::EXTShaderObjectExtensionName, vk::KHRRayQueryExtensionName, vk::KHRAccelerationStructureExtensionName,
    	vk::KHRDeferredHostOperationsExtensionName, vk::EXTHostImageCopyExtensionName, vk::KHRCopyCommands2ExtensionName,
        vk::KHRFormatFeatureFlags2ExtensionName, vk::KHRSynchronization2ExtensionName, vk::KHRMaintenance4ExtensionName };
    if constexpr (evk::isApple) dExtensions.emplace_back("VK_KHR_portability_subset");
    if (!evk::utils::extensionsOrLayersAvailable(physicalDevice.enumerateDeviceExtensionProperties(), dExtensions, [](const char* e) { std::printf("Extension not available: %s\n", e); })) exitWithError();

	// * activate features
    //vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceRayTracingPipelineFeaturesKHR> featuresChain;
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{ true };
    vk::PhysicalDeviceMaintenance4Features maintenance4Features{ true, &accelerationStructureFeatures };
    vk::PhysicalDeviceHostImageCopyFeaturesEXT hostImageCopyFeatures{ true, &maintenance4Features };
    vk::PhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{ true, &hostImageCopyFeatures };
    vk::PhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{ true, false, false, &rayQueryFeatures };
    vk::PhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures{ true, &bufferDeviceAddressFeatures };
    vk::PhysicalDeviceSynchronization2Features synchronization2Features{ true, &shaderObjectFeatures };
    vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2{ {}, &synchronization2Features };
    physicalDeviceFeatures2.features.shaderInt64 = true;
    // * create device
    auto device = evk::make_shared<evk::Device>(physicalDevice, dExtensions, evk::Device::Queues{ { queueFamilyIndex.value(), 1 } }, &physicalDeviceFeatures2);

    // Vertex buffer setup (triangle is upside down on purpose)
    const std::vector vertices = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    const size_t verticesSize = vertices.size() * sizeof(float);
    auto buffer = std::make_unique<evk::Buffer>(device, verticesSize, vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible); /* reBAR */
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

    // Image setup
    evk::Image image{ device, {target.width, target.height, 1}, vk::Format::eR8G8B8A8Snorm, vk::ImageTiling::eLinear,
        vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eHostTransferEXT, vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eHostVisible };
    image.transitionLayout(vk::ImageLayout::eGeneral);

    // Descriptor set setup
    evk::DescriptorSet descriptorSet{ device, evk::DescriptorSet::Bindings{
        { { 0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute } }
    } };
    descriptorSet.setDescriptor(0, vk::DescriptorImageInfo{ {}, image.imageView, vk::ImageLayout::eGeneral });
    descriptorSet.update();

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
    }, { pcRange }, shaderSpecialization, { descriptorSet } };

    cb.begin(vk::CommandBufferBeginInfo{});
    {
        cb.bindDescriptorSets(vk::PipelineBindPoint::eCompute, shader.layout, 0, { descriptorSet }, {});
        cb.bindShadersEXT(shader.stages, shader.shaders);
        cb.pushConstants<uint64_t>(*shader.layout, vk::ShaderStageFlagBits::eCompute, 0, tlas.deviceAddress);
        cb.dispatch(workGroupCount[0], workGroupCount[1], 1);
    }
    cb.end();
    device->getQueue(queueFamilyIndex.value(), 0).submitAndWaitIdle(vk::SubmitInfo{ {}, {}, *cb }, nullptr);

    // save as ppm
    std::vector<char> pixels(target.width * target.height * 4);
    image.copyImageToMemory(pixels.data());

    std::fstream fs("image.ppm", std::fstream::out);
    fs << "P3" << std::endl << target.width << " " << target.height << " 255" << std::endl;
    for (uint32_t i = 0; i < (target.width * target.height * 4u); i += 4u) {
        fs << static_cast<int>(pixels[i]) << " " << static_cast<int>(pixels[i + 1]) << " " << static_cast<int>(pixels[i + 2]) << " ";
    }
    return 0;
}
