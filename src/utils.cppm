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

        template<typename T, typename U>
        EVK_API void chain(T&& first, U&& second) {
            std::forward<T>(first).setPNext(&std::forward<U>(second));
        }

        template<typename T, typename U, typename ... Rest>
        EVK_API void chain(T&& first, U&& second, Rest&& ... rest) {
            std::forward<T>(first).setPNext(&std::forward<U>(second));
            chain(std::forward<U>(second), std::forward<Rest>(rest)...);
        }

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

    template<typename T>
    struct SharedPtr
    {
        EVK_API SharedPtr() : _ptr(nullptr) {}
        EVK_API SharedPtr(std::nullptr_t) : _ptr(nullptr) {}
        EVK_API SharedPtr(T* ptr) : _ptr(ptr) { increment(); }
        EVK_API SharedPtr(const SharedPtr& other) { copy(other._ptr); }
        EVK_API SharedPtr(const SharedPtr&& other) noexcept { copy(other._ptr); }
        EVK_API SharedPtr(const T&& data) { reset(new T(std::move(data))); }

        EVK_API SharedPtr& operator=(const SharedPtr& other)
        {
            if (this == &other) return *this;
            SharedPtr(std::move(other)).swap(*this);
            return *this;
        }
        EVK_API SharedPtr& operator=(SharedPtr&& other) noexcept
        {
            SharedPtr(std::move(other)).swap(*this);
            return *this;
        }

        EVK_API ~SharedPtr()
        {
#ifdef EVK_LOG_REF_COUNT
            if (_ptr) printf("Decrementing refCount: %d -> %d for %s %p\n", _ptr->refCount, _ptr->refCount - 1, typeid(T).name(), _ptr);
#endif
            decrement();
        }

        EVK_API T* operator->() { return _ptr; }
        EVK_API const T* operator->() const { return _ptr; }
        EVK_API T& operator*() { return *_ptr; }
        EVK_API const T& operator*() const { return *_ptr; }

        EVK_API operator const T& () const { return *_ptr; }

        EVK_API T* get() { return _ptr; }
        EVK_API const T* get() const { return _ptr; }
        EVK_API void reset(T* ptr = nullptr) { decrement(); _ptr = ptr; increment(); }
        EVK_API void swap(SharedPtr& other) noexcept
        {
            std::swap(_ptr, other._ptr);
        }
        EVK_API operator bool() const { return _ptr; }
    private:
        void increment()
        {
            if (_ptr) _ptr->refCount += 1;
        }
        void decrement()
        {
            if (_ptr) {
                _ptr->refCount -= 1;
                if (_ptr->refCount == 0) {
#ifdef EVK_LOG_REF_COUNT
                    printf("-> RIP %s\n", typeid(T).name());
#endif
                    delete _ptr;
                }
            }
        }
        void copy(T* ptr) { _ptr = ptr; increment(); }

        T* _ptr;
    };

    template <typename T, typename... Args>
    EVK_API evk::SharedPtr<T> make_shared(Args&&... args)
    {
        return SharedPtr<T>(new T(std::forward<Args>(args)...));
    }

    template<typename T>
    struct Shareable {
        template <typename... Args>
        static evk::SharedPtr<T> shared(Args&&... args) { return SharedPtr<T>(new T(std::forward<Args>(args)...)); }
    protected:
        uint32_t refCount = 0;
        friend struct SharedPtr<T>;
    };
}
