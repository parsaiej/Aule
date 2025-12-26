cmake_minimum_required(VERSION 3.28)

# Build System
# --------------------------------

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/External/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "")

set(PROJECT_NAME Aule)

project(${PROJECT_NAME} VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EXTERNAL_DIR ${CMAKE_SOURCE_DIR}/External/)

# Packages
# --------------------------------

find_package(volk CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

# ImGui:
# Needs to be compiled in a way that isn't vendored by vcpkg, so 
# we keep the submodule and compile in the sources directly.
# --------------------------------

file(GLOB IMGUI_SOURCES
    ${EXTERNAL_DIR}/imgui/*.cpp
    ${EXTERNAL_DIR}/imgui/backends/imgui_impl_vulkan.cpp
    ${EXTERNAL_DIR}/imgui/backends/imgui_impl_glfw.cpp
)

# Executable
# --------------------------------

add_library(${PROJECT_NAME}
    STATIC
        Source/Aule.cpp 
        ${IMGUI_SOURCES}
)

# PCH
# --------------------------------

target_precompile_headers(${PROJECT_NAME} 
    PUBLIC 
        Source/AulePrecompiled.h
)

# Includes
# --------------------------------

target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        ${EXTERNAL_DIR}/
        ${EXTERNAL_DIR}/imgui/
        ${EXTERNAL_DIR}/imgui/backends
)

# Link
# --------------------------------

target_link_libraries(${PROJECT_NAME} 
    PUBLIC 
        volk::volk 
        glfw
        GPUOpen::VulkanMemoryAllocator
)

# Compile flags
# --------------------------------

target_compile_definitions(${PROJECT_NAME}
    PUBLIC
        IMGUI_DEFINE_MATH_OPERATORS
        IMGUI_IMPL_VULKAN_USE_VOLK
)

# Sample
# --------------------------------

add_subdirectory(Sample)