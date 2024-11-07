#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

// test application to showcase a memory leak
int main(int /*argc*/, char** /*argv*/) {

    static vk::DynamicLoader dl{};
    auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    // Create instance
    vk::ApplicationInfo appInfo;
    appInfo.setApiVersion(VK_API_VERSION_1_3);

    std::vector layers{ "VK_LAYER_KHRONOS_validation" };
    vk::InstanceCreateInfo instanceInfo;
    instanceInfo.setPApplicationInfo(&appInfo);
    instanceInfo.setPEnabledLayerNames(layers);
    auto instance = vk::createInstance(instanceInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);

    // Pick first gpu
    auto physicalDevice = instance.enumeratePhysicalDevices().front();
    const float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo;
    queueCreateInfo.setQueueFamilyIndex(0);
    queueCreateInfo.setQueuePriorities(queuePriority);

    vk::DeviceCreateInfo deviceInfo;
    deviceInfo.setQueueCreateInfos(queueCreateInfo);

    auto device = physicalDevice.createDevice(deviceInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

    auto queue = device.getQueue(0, 0);
    vk::CommandPoolCreateInfo commandPoolInfo;
    commandPoolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    commandPoolInfo.setQueueFamilyIndex(0);
    auto commandPool = device.createCommandPool(commandPoolInfo);

    // when reusing a commandbuffer which should reset implicitely, the memory is not freed
    // explicitely reseting it will free the memory
    vk::CommandBufferAllocateInfo commandBufferInfo;
    auto cb = device.allocateCommandBuffers(commandBufferInfo.setCommandPool(commandPool).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1)).front();
    while (true) {
        /* hpp version */
        //cb.reset(); // memory leak fix
        cb.begin(vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
        cb.end();
        queue.submit(vk::SubmitInfo{ {}, {}, cb }, nullptr);
        device.waitIdle();

        /* h version */
        //auto vkcb = (VkCommandBuffer)cb;
        ////VULKAN_HPP_DEFAULT_DISPATCHER.vkResetCommandBuffer(vkcb, {}); // memory leak fix
        //auto cbBeginInfo = VkCommandBufferBeginInfo{};
        //cbBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        //cbBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        //VULKAN_HPP_DEFAULT_DISPATCHER.vkBeginCommandBuffer(cb, &cbBeginInfo);
        //VULKAN_HPP_DEFAULT_DISPATCHER.vkEndCommandBuffer(cb);

        //auto submitInfo = VkSubmitInfo{};
        //submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        //submitInfo.commandBufferCount = 1;
        //submitInfo.pCommandBuffers = &vkcb;
        //VULKAN_HPP_DEFAULT_DISPATCHER.vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        //VULKAN_HPP_DEFAULT_DISPATCHER.vkDeviceWaitIdle(device);
    }

    return 0;
}
