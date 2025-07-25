cmake_minimum_required(VERSION 3.28)
cmake_policy(SET CMP0069 NEW) 
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

project(evk)
#================================#
# VARS                           #
#================================#
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
# dirs
set(EVK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(EVK_VULKAN_HEADERS_DIR "${EVK_SOURCE_DIR}/vulkan-headers")
# folders
set(EVK_EXTERNAL_FOLDER "Dependencies")
set(EVK_EXAMPLES_FOLDER "Examples")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(EVK_IS_TOPLEVEL_PROJECT TRUE)
    set(EVK_INCLUDE_IMGUI_BACKEND TRUE)
else()
    set(EVK_IS_TOPLEVEL_PROJECT FALSE)
endif()
option(EVK_BUILD_EXAMPLES "Build EXAMPLES" ${EVK_IS_TOPLEVEL_PROJECT})

include(FetchContent)
#================================#
# VULKAN-HEADERS                 #
#================================#
# FetchContent_Populate(vulkan-headers URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/heads/main.zip QUIET)
FetchContent_Populate(vulkan-headers URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.4.323.zip QUIET)
message(STATUS "Vulkan-Headers dir: ${vulkan-headers_SOURCE_DIR}")

#================================#
# EVK                            #
#================================#
add_library(${PROJECT_NAME} SHARED)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_sources(${PROJECT_NAME}
    PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${vulkan-headers_SOURCE_DIR}" FILES "${vulkan-headers_SOURCE_DIR}/include/vulkan/vulkan.cppm"
    PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES "src/evk.cppm" "src/core.cppm" "src/rt.cppm" "src/utils.cppm"
    PRIVATE "src/core.cpp" "src/rt.cpp" "src/utils.cpp"
)

target_include_directories(${PROJECT_NAME} PUBLIC "${vulkan-headers_SOURCE_DIR}/include")
target_compile_definitions(${PROJECT_NAME} PUBLIC 
    VULKAN_HPP_NO_SMART_HANDLE
    VULKAN_HPP_NO_SPACESHIP_OPERATOR
    VULKAN_HPP_NO_TO_STRING
    VK_NO_PROTOTYPES
)
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_WIN32_KHR NOMINMAX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "EVK_API=__declspec(dllexport)" INTERFACE "EVK_API=__declspec(dllimport)")
elseif(APPLE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_METAL_EXT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "EVK_API=" INTERFACE "EVK_API=")
    find_package(Vulkan REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)
elseif(UNIX)
    target_compile_definitions(${PROJECT_NAME} PUBLIC VK_USE_PLATFORM_XLIB_KHR)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "EVK_API=" INTERFACE "EVK_API=")
endif()

# optional imgui backend
# window backends need to be included in the main project
if(EVK_INCLUDE_IMGUI_BACKEND) # imgui
    set(EVK_INCLUDE_IMGUI_VERSION "v1.92.1" CACHE STRING "ImGui version")
    FetchContent_Populate(imgui URL https://github.com/ocornut/imgui/archive/refs/tags/${EVK_INCLUDE_IMGUI_VERSION}.zip QUIET)
    message(STATUS "ImGui-Source dir: ${imgui_SOURCE_DIR}")
    if(NOT EVK_IS_TOPLEVEL_PROJECT)
        set(EVK_IMGUI_SOURCE_DIR "${imgui_SOURCE_DIR}" PARENT_SCOPE)
    endif()
    # sources files
    set(EVK_IMGUI_SOURCES "${imgui_SOURCE_DIR}/imgui.cpp" "${imgui_SOURCE_DIR}/imgui_widgets.cpp" "${imgui_SOURCE_DIR}/imgui_draw.cpp" 
        "${imgui_SOURCE_DIR}/imgui_tables.cpp" "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp")

    source_group(TREE "${imgui_SOURCE_DIR}" PREFIX "ImGui (External)" FILES ${EVK_IMGUI_SOURCES})
    # target
    target_sources(${PROJECT_NAME} PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES "src/imgui_backend.cppm" PRIVATE "src/imgui_backend.cpp" ${EVK_IMGUI_SOURCES})
    target_include_directories(${PROJECT_NAME} PUBLIC "${imgui_SOURCE_DIR}")# "${imgui_SOURCE_DIR}/backends")
    
    target_compile_definitions(${PROJECT_NAME} PRIVATE "IMGUI_IMPL_API="  INTERFACE "IMGUI_IMPL_API=")
    if(WIN32)
        target_compile_definitions(${PROJECT_NAME} PRIVATE "IMGUI_API=__declspec(dllexport)" INTERFACE "IMGUI_API=__declspec(dllimport)")
    elseif(APPLE)
        target_compile_definitions(${PROJECT_NAME} PRIVATE "IMGUI_API=" INTERFACE "IMGUI_API=")
    endif()
endif()

install(TARGETS ${PROJECT_NAME} CONFIGURATIONS Debug DESTINATION "debug")
install(TARGETS ${PROJECT_NAME} CONFIGURATIONS Release DESTINATION "release")

if(EVK_BUILD_EXAMPLES)
    # <glfw>
    FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/fknfilewalker/glfw GIT_TAG macos_metal_layer)
    set(GLFW_BUILD_EXAMPLES OFF)
    set(GLFW_BUILD_TESTS OFF)
    set(GLFW_BUILD_DOCS OFF)
    set(GLFW_INSTALL OFF)
    FetchContent_MakeAvailable(glfw)
    set_target_properties(glfw update_mappings PROPERTIES FOLDER "${EVK_EXTERNAL_FOLDER}/glfw")
    target_compile_definitions(glfw INTERFACE "GLFW_INCLUDE_NONE")
    if(WIN32)
        target_compile_definitions(glfw INTERFACE "GLFW_EXPOSE_NATIVE_WIN32")
    elseif(APPLE)
        target_compile_definitions(glfw INTERFACE "GLFW_EXPOSE_NATIVE_COCOA")
    elseif(UNIX)
        target_compile_definitions(glfw INTERFACE "GLFW_EXPOSE_NATIVE_X11")
    endif()
    # </glfw>

    function(add_target target)
        cmake_parse_arguments(PARSE_ARGV 1 TARGET "" "" "DEPS;SOURCES")
        add_executable(${target} ${TARGET_SOURCES})
        set_target_properties(${target} PROPERTIES FOLDER ${EVK_EXAMPLES_FOLDER})
        if(TARGET_DEPS)
            target_link_libraries(${target} PRIVATE ${TARGET_DEPS})
            add_dependencies(${target} ${TARGET_DEPS})
        endif()
        if(MSVC)
            set_target_properties(${target} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        endif()
        install(TARGETS ${target} CONFIGURATIONS Debug DESTINATION "debug")
        install(TARGETS ${target} CONFIGURATIONS Release DESTINATION "release")
    endfunction(add_target target)

    add_target(rasterizer_triangle DEPS ${PROJECT_NAME} glfw SOURCES "examples/rasterizer_triangle/main.cpp" "examples/rasterizer_triangle/shaders.h" "examples/rasterizer_triangle/triangle.vert" "examples/rasterizer_triangle/triangle.frag")
    add_target(ray_query_triangle DEPS ${PROJECT_NAME} glfw SOURCES "examples/ray_query_triangle/main.cpp" "examples/ray_query_triangle/shaders.h" "examples/ray_query_triangle/triangle.comp")
    add_target(headless_ray_query_triangle DEPS ${PROJECT_NAME} SOURCES "examples/headless_ray_query_triangle/main.cpp" "examples/headless_ray_query_triangle/shaders.h" "examples/headless_ray_query_triangle/triangle.comp")
    if(EVK_INCLUDE_IMGUI_BACKEND) 
        set(EVK_IMGUI_BACKEND_SOURCES "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp")
        source_group(TREE "${imgui_SOURCE_DIR}" PREFIX "ImGui (External)" FILES ${EVK_IMGUI_BACKEND_SOURCES})
        add_target(imgui_rasterizer_triangle DEPS ${PROJECT_NAME} glfw SOURCES "examples/imgui_rasterizer_triangle/main.cpp" "examples/imgui_rasterizer_triangle/shaders.h" "examples/imgui_rasterizer_triangle/triangle.vert" "examples/imgui_rasterizer_triangle/triangle.frag" ${EVK_IMGUI_BACKEND_SOURCES})
    endif()
    
    add_target(experiments DEPS ${PROJECT_NAME} glfw SOURCES "examples/experiments/main.cpp")
    add_target(bug DEPS ${PROJECT_NAME} SOURCES "examples/bug/main.cpp")
endif()
