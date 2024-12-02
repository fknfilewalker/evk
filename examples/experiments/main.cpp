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


constexpr uint32_t window_width = 800;
constexpr uint32_t window_height = 600;
int main(int /*argc*/, char** /*argv*/)
{
    
    return 0;
}
