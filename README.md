## Aule

Simple middleware library for fast creation of a cross-platform graphics runtime (Vulkan), operating system window (GLFW), and user interface (ImGui). 

## Usage

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

## Build

## Integrate
