module;
#include <algorithm>
#include <bitset>
#include <optional>
#include <functional>
#include <stdint.h>
module evk;
import :utils;
using namespace evk;

template<typename T>
bool utils::extensionOrLayerAvailable(const std::vector<T>& available, const char* requested) {
    static_assert(std::is_same_v<vk::LayerProperties, T> || std::is_same_v<vk::ExtensionProperties, T>);
    return std::find_if(available.begin(), available.end(), [requested](const T& availableElement) {
        if constexpr (std::is_same_v<vk::LayerProperties, T>) return std::string_view{ availableElement.layerName.data() }.compare(requested) == 0;
        else if constexpr (std::is_same_v<vk::ExtensionProperties, T>) return std::string_view{ availableElement.extensionName.data() }.compare(requested) == 0;
    }) != available.end();
}
template EVK_API bool utils::extensionOrLayerAvailable<vk::LayerProperties>(const std::vector<vk::LayerProperties>&, const char*);
template EVK_API bool utils::extensionOrLayerAvailable<vk::ExtensionProperties>(const std::vector<vk::ExtensionProperties>&, const char*);

template<typename T>
bool utils::extensionsOrLayersAvailable(const std::vector<T>& available, const std::vector<const char*>& requested, const std::function<void(const char*)>& notAvailableCallback) {
    static_assert(std::is_same_v<vk::LayerProperties, T> || std::is_same_v<vk::ExtensionProperties, T>);
    bool allAvailable = true;
    for (const char* r : requested) {
        if (!extensionOrLayerAvailable(available, r)) {
            allAvailable &= false;
            if (notAvailableCallback) notAvailableCallback(r);
        }
    }
    return allAvailable;
}
template EVK_API bool utils::extensionsOrLayersAvailable<vk::LayerProperties>(const std::vector<vk::LayerProperties>&, const std::vector<const char*>&, const std::function<void(const char*)>&);
template EVK_API bool utils::extensionsOrLayersAvailable<vk::ExtensionProperties>(const std::vector<vk::ExtensionProperties>&, const std::vector<const char*>&, const std::function<void(const char*)>&);

template <typename T>
bool utils::addExtOrLayerIfAvailable(std::vector<const char*>& extensions, const std::vector<T>& available, const char* requested)
{
    static_assert(std::is_same_v<vk::LayerProperties, T> || std::is_same_v<vk::ExtensionProperties, T>);
    if (utils::extensionOrLayerAvailable(available, requested)) {
        extensions.push_back(requested);
        return true;
    }
    return false;
}
template EVK_API bool utils::addExtOrLayerIfAvailable<vk::LayerProperties>(std::vector<const char*>&, const std::vector<vk::LayerProperties>&, const char*);
template EVK_API bool utils::addExtOrLayerIfAvailable<vk::ExtensionProperties>(std::vector<const char*>&, const std::vector<vk::ExtensionProperties>&, const char*);

template <typename T>
void utils::addExtsOrLayersIfAvailable(std::vector<const char*>& extensions, const std::vector<T>& available, const std::vector<const char*>& requested, const std::function<void(const char*)>& notAvailableCallback)
{
    static_assert(std::is_same_v<vk::LayerProperties, T> || std::is_same_v<vk::ExtensionProperties, T>);
    for (const char* r : requested) if (!utils::addExtOrLayerIfAvailable(extensions, available, r) && notAvailableCallback) notAvailableCallback(r);
}
template EVK_API void utils::addExtsOrLayersIfAvailable<vk::LayerProperties>(std::vector<const char*>&, const std::vector<vk::LayerProperties>&, const std::vector<const char*>&, const std::function<void(const char*)>&);
template EVK_API void utils::addExtsOrLayersIfAvailable<vk::ExtensionProperties>(std::vector<const char*>&, const std::vector<vk::ExtensionProperties>&, const std::vector<const char*>&, const std::function<void(const char*)>&);

template<typename T>
bool utils::remExtOrLayerIfNotAvailable(std::vector<const char*>& extensions, const std::vector<T>& available, const char* requested)
{
    static_assert(std::is_same_v<vk::LayerProperties, T> || std::is_same_v<vk::ExtensionProperties, T>);
    const auto oldSize = extensions.size();
    std::vector<const char*> out;
    out.reserve(oldSize);
    for (const char* r : extensions) utils::addExtOrLayerIfAvailable(out, available, r);
    extensions = out;
    return out.size() != oldSize;
}

template EVK_API bool utils::remExtOrLayerIfNotAvailable<vk::LayerProperties>(std::vector<const char*>&, const std::vector<vk::LayerProperties>&, const char*);
template EVK_API bool utils::remExtOrLayerIfNotAvailable<vk::ExtensionProperties>(std::vector<const char*>&, const std::vector<vk::ExtensionProperties>&, const char*);


template <typename T>
void utils::remExtsOrLayersIfNotAvailable(std::vector<const char*>& extensions, const std::vector<T>& available, const std::function<void(const char*)>& removeCallback)
{
    static_assert(std::is_same_v<vk::LayerProperties, T> || std::is_same_v<vk::ExtensionProperties, T>);
    std::vector<const char*> out;
    out.reserve(extensions.size());
    for (const char* r : extensions) if (!utils::addExtOrLayerIfAvailable(out, available, r) && removeCallback) removeCallback(r);
    extensions = out;
}
template EVK_API void utils::remExtsOrLayersIfNotAvailable<vk::LayerProperties>(std::vector<const char*>&, const std::vector<vk::LayerProperties>&, const std::function<void(const char*)>&);
template EVK_API void utils::remExtsOrLayersIfNotAvailable<vk::ExtensionProperties>(std::vector<const char*>&, const std::vector<vk::ExtensionProperties>&, const std::function<void(const char*)>&);


std::optional<uint32_t> utils::findQueueFamilyIndex(
    const std::vector<vk::QueueFamilyProperties>& queueFamiliesProperties, 
    const vk::QueueFlags queueFlags,
    const std::vector<uint32_t>& ignoreFamilies)
{
    std::optional<uint32_t> bestFamily;
    std::bitset<12> bestScore = 0;
    for (uint32_t i = 0; i < queueFamiliesProperties.size(); i++) {
        if (std::ranges::find(ignoreFamilies, i) != ignoreFamilies.end()) continue;
        // check if queue family supports all requested queue flags
        if (static_cast<uint32_t>(queueFamiliesProperties[i].queueFlags & queueFlags) == static_cast<uint32_t>(queueFlags)) {
            const std::bitset<12> score = static_cast<uint32_t>(queueFamiliesProperties[i].queueFlags);
            // use queue family with the least other bits set
            if (!bestFamily.has_value() || score.count() < bestScore.count()) {
                bestFamily = i;
                bestScore = score;
            }
        }
    }
    return bestFamily;
}

std::optional<uint32_t> utils::findMemoryTypeIndex(
    const vk::PhysicalDeviceMemoryProperties& memoryProperties, 
    const vk::MemoryRequirements& requirements, 
    const vk::MemoryPropertyFlags propertyFlags)
{
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((requirements.memoryTypeBits & (1u << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags)) return i;
    }
    return std::nullopt;
}

vk::ImageType utils::extentToImageType(const vk::Extent3D& extent)
{
	if (extent.depth > 0) return vk::ImageType::e3D;
	if (extent.height > 0) return vk::ImageType::e2D;
	return vk::ImageType::e1D;
}

vk::ImageViewType utils::extentToImageViewType(const vk::Extent3D& extent)
{
	if (extent.depth > 0) return vk::ImageViewType::e3D;
	if (extent.height > 0) return vk::ImageViewType::e2D;
	return vk::ImageViewType::e1D;
}

vk::ImageAspectFlags utils::formatToAspectMask(const vk::Format format)
{
    switch (format) {
	    case vk::Format::eD16Unorm:
	    case vk::Format::eD32Sfloat:
	        return vk::ImageAspectFlagBits::eDepth;
	    case vk::Format::eS8Uint:
	        return vk::ImageAspectFlagBits::eStencil;
	    case vk::Format::eD16UnormS8Uint:
	    case vk::Format::eD24UnormS8Uint:
	    case vk::Format::eD32SfloatS8Uint:
	        return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
	    default:
	        return vk::ImageAspectFlagBits::eColor;
    }
}
