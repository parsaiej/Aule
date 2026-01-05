cmake_minimum_required(VERSION 3.28)

# Build System
# --------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXTERNAL_DIR ${CMAKE_CURRENT_LIST_DIR}/External/)

# Packages
# --------------------------------

# Prefer this over vcpkg since vcpkg is poorly designed for modular middleware.
include(FetchContent)

FetchContent_Declare(volk  GIT_REPOSITORY https://github.com/zeux/volk.git                                      GIT_TAG master)
FetchContent_Declare(glfw  GIT_REPOSITORY https://github.com/glfw/glfw.git                                      GIT_TAG 3.4)
FetchContent_Declare(vma   GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git GIT_TAG v3.3.0)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git                                  GIT_TAG v1.92.5)

set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS    OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS     OFF CACHE INTERNAL "")
set(GLFW_INSTALL        OFF CACHE INTERNAL "")
set(GLFW_BUILD_X11      OFF CACHE BOOL     "")
set(GLFW_BUILD_WAYLAND  ON  CACHE BOOL     "")

FetchContent_MakeAvailable(volk glfw vma imgui)

# ImGui:
# Needs to be compiled in a way that isn't vendored by vcpkg, so 
# we keep the submodule and compile in the sources directly.
# --------------------------------

file(GLOB IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/*.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
)

# Executable
# --------------------------------

add_library(Aule
    STATIC
        Source/Aule.cpp 
        Source/AulePrecompiled.cpp 
        ${IMGUI_SOURCES}
)

# PCH
# --------------------------------

target_precompile_headers(Aule
    PUBLIC 
        Source/AulePrecompiled.h
)

# Includes
# --------------------------------

target_include_directories(Aule
    PUBLIC 
        ${imgui_SOURCE_DIR}/
        ${imgui_SOURCE_DIR}/backends
)

# Link
# --------------------------------

target_link_libraries(Aule
    PUBLIC 
        volk::volk 
        glfw
        GPUOpen::VulkanMemoryAllocator
)

# Compile flags
# --------------------------------

target_compile_definitions(Aule
    PUBLIC
        IMGUI_DEFINE_MATH_OPERATORS
        IMGUI_IMPL_VULKAN_USE_VOLK
)

# Sample
# --------------------------------

add_subdirectory(Sample)