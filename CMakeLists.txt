cmake_minimum_required(VERSION 3.24)
project(PathTracer VERSION 1.0.0 LANGUAGES CXX)

option(PT_BUILD_TESTS "Build the CPU unit tests" ON)
option(PT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(PT_ENABLE_CLANG_TIDY "Run clang-tidy (using .clang-tidy) while building" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------------------------
# Dependencies
#   - Vulkan SDK (loader, headers, glslc): https://vulkan.lunarg.com
#   - glfw3, glm, VulkanMemoryAllocator, stb: vcpkg.json, or system packages on Linux
#   - Dear ImGui and tinyobjloader are vendored in external/
# ---------------------------------------------------------------------------------------------
find_package(Vulkan 1.3 REQUIRED COMPONENTS glslc)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
# vcpkg and the Vulkan SDK install VMA as include/vma/vk_mem_alloc.h, Linux distros as
# include/vk_mem_alloc.h; search both.
find_path(VMA_INCLUDE_DIR vk_mem_alloc.h
    PATH_SUFFIXES vma
    HINTS $ENV{VULKAN_SDK}/Include $ENV{VULKAN_SDK}/include
    REQUIRED)
find_path(STB_INCLUDE_DIR stb_image_write.h PATH_SUFFIXES stb REQUIRED)

# Third-party code is compiled in its own targets, without the project's warning flags, and its
# headers are SYSTEM includes so they don't produce warnings in our code either.
add_library(pt_imgui STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_vulkan.cpp
)
target_include_directories(pt_imgui SYSTEM PUBLIC external/imgui)
target_link_libraries(pt_imgui PUBLIC Vulkan::Vulkan glfw)

add_library(pt_thirdparty STATIC
    src/external/stb_image_write.cpp
    src/external/tinyobjloader.cpp
)
target_include_directories(pt_thirdparty SYSTEM PUBLIC external ${STB_INCLUDE_DIR})

add_library(pt_vma STATIC src/external/vma.cpp)
target_include_directories(pt_vma SYSTEM PUBLIC ${VMA_INCLUDE_DIR})
target_link_libraries(pt_vma PUBLIC Vulkan::Vulkan)

function(pt_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8
            $<$<BOOL:${PT_WARNINGS_AS_ERRORS}>:/WX>)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic
            $<$<BOOL:${PT_WARNINGS_AS_ERRORS}>:-Werror>)
    endif()
    if(PT_ENABLE_CLANG_TIDY)
        find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    endif()
endfunction()

# ---------------------------------------------------------------------------------------------
# pt_scene: CPU-only scene code (BVH, scene compilation, presets, OBJ loading). No Vulkan, so
# the unit tests can link it without a GPU.
# ---------------------------------------------------------------------------------------------
add_library(pt_scene STATIC
    src/renderer/SceneCompiler.cpp
    src/scene/Bvh.cpp
    src/scene/Camera.cpp
    src/scene/MeshLoader.cpp
    src/scene/ScenePresets.cpp
    src/utils/Paths.cpp
)
target_include_directories(pt_scene PUBLIC include)
target_link_libraries(pt_scene PUBLIC glm::glm PRIVATE pt_thirdparty)
target_compile_definitions(pt_scene PRIVATE PT_SOURCE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
pt_set_warnings(pt_scene)

# ---------------------------------------------------------------------------------------------
# PathTracer: the application
# ---------------------------------------------------------------------------------------------
add_executable(PathTracer
    src/main.cpp
    src/app/Application.cpp
    src/app/CameraController.cpp
    src/app/CommandLine.cpp
    src/core/Barriers.cpp
    src/core/ComputePipeline.cpp
    src/core/Device.cpp
    src/core/Frame.cpp
    src/core/Swapchain.cpp
    src/core/VkCheck.cpp
    src/core/VulkanContext.cpp
    src/renderer/GpuScene.cpp
    src/renderer/ImGuiLayer.cpp
    src/renderer/PathTracer.cpp
    src/renderer/Renderer.cpp
    src/resources/Buffer.cpp
    src/resources/Descriptors.cpp
    src/resources/Image.cpp
    src/window/Window.cpp
)
target_link_libraries(PathTracer PRIVATE pt_scene pt_imgui pt_thirdparty pt_vma Vulkan::Vulkan glfw glm::glm)
pt_set_warnings(PathTracer)

# Single output directory for every configuration: the app finds shaders/ next to itself, so it
# runs the same from an IDE, a shell, or a double-click.
set(PT_BIN_DIR ${CMAKE_BINARY_DIR}/bin)
set_target_properties(PathTracer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PT_BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PT_BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PT_BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${PT_BIN_DIR}
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${PT_BIN_DIR}
)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT PathTracer)

# ---------------------------------------------------------------------------------------------
# Shaders: compiled to SPIR-V at build time, with #include dependency tracking via depfiles.
# ---------------------------------------------------------------------------------------------
set(SHADER_SOURCES
    shaders/pathtrace.comp
    shaders/resolve.comp
)
set(SHADER_BINARIES)
foreach(source IN LISTS SHADER_SOURCES)
    get_filename_component(name ${source} NAME)
    set(output ${PT_BIN_DIR}/shaders/${name}.spv)
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PT_BIN_DIR}/shaders
        COMMAND Vulkan::glslc --target-env=vulkan1.3 -O -Werror
                -MD -MF ${output}.d -o ${output} ${CMAKE_SOURCE_DIR}/${source}
        DEPENDS ${CMAKE_SOURCE_DIR}/${source}
        DEPFILE ${output}.d
        COMMENT "Compiling ${name}"
        VERBATIM
    )
    list(APPEND SHADER_BINARIES ${output})
endforeach()
add_custom_target(Shaders ALL DEPENDS ${SHADER_BINARIES} SOURCES ${SHADER_SOURCES})
add_dependencies(PathTracer Shaders)

# ---------------------------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------------------------
if(PT_BUILD_TESTS)
    enable_testing()
    add_executable(pt_tests tests/CoreTests.cpp)
    target_link_libraries(pt_tests PRIVATE pt_scene)
    pt_set_warnings(pt_tests)
    add_test(NAME core COMMAND pt_tests)
endif()
