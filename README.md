## Aule

Aule is a cross-platform rapid Vulkan C++ prototyping tool. 

It allows you to get quickly up and running with a cross-platform graphics runtime that draws to an operating system window, with helpful tools ready to use, like ImGui and the Vulkan Memory Allocator. 

Aule is for folks who would rather work directly with the graphics API instead of an abstraction, but want to skip all the annoying parts of getting that environment up and running. Kind of like vk-bootstrap but it assumes you want to draw to an operating system window.

Just specify some initialization parameters, provide your render lambda, and you are good to go. 

## Example

This image was produced on a macOS device using the C++ code sample below it. See also the Sample/ directory.

<img width="1274" height="747" alt="image" src="https://github.com/user-attachments/assets/7d65c6a3-701e-40aa-9690-bd108f6cb804" />

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

## Integrate
