## `Aule`

Aule is a cross-platform rapid Vulkan C++ prototyping tool. 

Aule lets you skip the process of:
- Basic Vulkan initialization (Instance, Physical Device, Logical Device, Queue Enumeration)
- Cross-platform operating system window creation.
- Swapchain creation and configuration for OS window.
- Frame pacing and synchronization, swapchain presentation.
- Configuring ImGui
- Configuring Vulkan Memory Allocator

Aule is for folks who would rather work directly with the graphics API instead of an abstraction, but want to skip all the annoying parts of getting that environment up and running. Kind of like [vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap) but it assumes you want to draw to an operating system window, speeding things up even more.

## Basic Usage

See the full code listing at [Sample/Main.cpp](https://github.com/parsaiej/Aule/blob/master/Sample/Main.cpp):

```
#include "../Include/Aule/Aule.h"

int main(int argc, char** argv)
{
    Aule::Params params = {};
    {
        params.windowName   = "Aule Sample";
        params.windowWidth  = 1280u;
        params.windowHeight = 720u;
    }

    auto context = Aule::CreateContext(params);

    // Use the context to create shaders, upload data, arrange descriptors...
    // ....

    // Kick off the render loop. Frame pacing, queue submission, swapchain presentation, and synchronization
    // are handled automatically. Your lambda is provided the active frame index which can be
    // used to record work into the current command buffer and draw to the active swapchain image.
    Aule::Dispatch(context,
                   [&](uint32_t frameIndex)
                   {
                       auto& cmd = context.frameCommandBuffer[frameIndex];
                       auto& buf = context.swapchainImages[frameIndex];

                       // Barriers ...

                       VkImageSubresourceRange clearSubresourceRange = {};
                       {
                           clearSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                           clearSubresourceRange.layerCount = 1u;
                           clearSubresourceRange.levelCount = 1u;
                       }

                       VkClearColorValue clearColor = { 1, 0, 0, 0 };

                       vkCmdClearColorImage(
                          cmd, buf,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          &clearColor,
                          1u, &clearSubresourceRange
                       );

                       // Barriers ...

                       // Record UI for your application.
                       ImGui::ShowDemoWindow();
                   });

    Aule::DestroyContext(context);

    return 0;
}
```

The above code snipped was used to compile this application running on an Apple Silicon device:

<img width="1274" height="747" alt="image" src="https://github.com/user-attachments/assets/7d65c6a3-701e-40aa-9690-bd108f6cb804" />

## Setting up `Aule`

The simplest way to use Aule is by adding it as a submodule to your project.

`git submodule add https://github.com/parsaiej/Aule.git`

Currently Aule is only supported by CMake build system due to how the dependency resolution works. Add the following to your CMakeLists.txt:

`add_subdirectory(Aule)`

Next, link with Aule:

`target_link_libraries(YourProject PRIVATE Aule)`

And include it:

`target_include_directories(YourProject PRIVATE ${CMAKE_SOURCE_DIR}/Aule/Include)`

Finally, just include the header in your cpp file:

`#include <Aule/Aule.h>`

Your project will inherit Aule's precompiled header which includes utility headers like ImGui and Vulkan Memory Allocator.


## Dependencies

Aule uses the following middlewares which will be auto-pulled by CMake when you follow the above instructions:
- [ImGui](https://github.com/ocornut/imgui)
- [volk](https://github.com/zeux/volk)
- [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [GLFW](https://github.com/glfw/glfw)

## Internal Extensions

Aule uses the following device extensions internally:
- `VK_KHR_swapchain`
- `VK_KHR_dynamic_rendering`
- `VK_KHR_synchronization2`

Additionally it will use the instance extensions returned by GLFW's `glfwGetRequiredInstanceExtensions`

You can specify additional device extensions to load in the Aule::Params. 
