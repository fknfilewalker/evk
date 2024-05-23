module;
#include <memory>
#include <optional>
#include <vector>
#include <concepts>
#include <functional>
export module evk:utils;
import vulkan_hpp;

export namespace evk {
    constexpr bool isDebug =
#if !defined( NDEBUG ) || defined( _DEBUG )
        true
#else 
        false
#endif
        ;

    constexpr enum class OS { Windows, Linux, MacOS } os =
#ifdef _WIN32
        OS::Windows
#elif __linux__
        OS::Linux
#elif __APPLE__
        OS::MacOS
#endif
        ;
    constexpr bool isWindows = os == OS::Windows;
    constexpr bool isLinux = os == OS::Linux;
    constexpr bool isMacOS = os == OS::MacOS;
    constexpr bool isApple = isMacOS;

    namespace utils {
        template <typename T>
        concept numerical = std::integral<T> || std::floating_point<T>;

        template<numerical T>
        constexpr T roundUpToMultipleOf(const T numToRound, const T multiple)
    	{
            return ((numToRound + multiple - 1) / multiple) * multiple;
        }

        template<std::unsigned_integral T>
    	constexpr size_t roundUpToMultipleOfPowerOf2(const T numToRound, const T powerOf2)
        {
	        return (numToRound + powerOf2 - 1) & -powerOf2;
        }
        template<std::unsigned_integral T>
        constexpr size_t roundDownTooMultipleOfPowerOf2(const T numToRound, const T powerOf2)
        {
	        return numToRound & ~(powerOf2 - 1);
        }

        template<std::unsigned_integral T> constexpr T areBitsSet(const T bitfield, const T bits) { return (bitfield & bits) == bits; }
        template<std::unsigned_integral T> constexpr T areAnyBitsSet(const T bitfield, const T bits) { return (bitfield & bits) > 0u; }

        template<typename T> EVK_API bool extensionOrLayerAvailable(const std::vector<T>& available, const char* requested);
        template<typename T> EVK_API bool extensionsOrLayersAvailable(const std::vector<T>& available, const std::vector<const char*>& requested, const std::function<void(const char*)>& notAvailableCallback = {});
        template<typename T> EVK_API bool addExtOrLayerIfAvailable(std::vector<const char*>& extensions, const std::vector<T>& available, const char* requested);
        template<typename T> EVK_API void addExtsOrLayersIfAvailable(std::vector<const char*>& extensions, const std::vector<T>& available, const std::vector<const char*>& requested, const std::function<void(const char*)>& notAvailableCallback = {});
        template<typename T> EVK_API bool remExtOrLayerIfNotAvailable(std::vector<const char*>& extensions, const std::vector<T>& available, const char* requested);
        template<typename T> EVK_API void remExtsOrLayersIfNotAvailable(std::vector<const char*>& extensions, const std::vector<T>& available, const std::function<void(const char*)>& removeCallback = {});


        EVK_API std::optional<uint32_t> findQueueFamilyIndex(
            const std::vector<vk::QueueFamilyProperties>& queueFamiliesProperties,
            vk::QueueFlags queueFlags,
            const std::vector<uint32_t>& ignoreFamilies = {}
        );
        EVK_API [[nodiscard]] std::optional<uint32_t> findMemoryTypeIndex(
            const vk::PhysicalDeviceMemoryProperties& memoryProperties,
            const vk::MemoryRequirements& requirements,
            vk::MemoryPropertyFlags propertyFlags
        );

        EVK_API vk::ImageType extentToImageType(
            const vk::Extent3D& extent
        );
        EVK_API vk::ImageViewType extentToImageViewType(
            const vk::Extent3D& extent
        );
        EVK_API vk::ImageAspectFlags formatToAspectMask(
            vk::Format format
        );

        EVK_API const uint32_t& clampSwapchainImageCount(const uint32_t& count, const vk::SurfaceCapabilitiesKHR& capabilities)
        {
            return std::max(capabilities.maxImageCount ? std::min(count, capabilities.maxImageCount) : count, capabilities.minImageCount);
        }
    }

}