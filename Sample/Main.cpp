#include "vulkan/vulkan_core.h"
#include <Aule/Aule.h>

#include <stdexcept>
#include <iostream>

int main(int argc, char** argv)
{
    Aule::Params params = {};
    {
        params.windowName        = "Aule Sample";
        params.windowWidth       = 1280u;
        params.windowHeight      = 720u;
        params.numFramesInFlight = 3u;
    }

    try
    {
        auto context = Aule::Initialize(params);

        // Perform initialization of resources with the context.

        // Invoke the renderloop.
        Aule::Dispatch(
            [&](uint32_t swapchainIndex, uint32_t frameIndex)
            {
                auto& cmd = context.frameCommandBuffer[frameIndex];

                // -----

                // -----

                VkImageSubresourceRange clearSubresourceRange = {};
                {
                    clearSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    clearSubresourceRange.layerCount = 1u;
                    clearSubresourceRange.levelCount = 1u;
                }

                VkClearColorValue clearColor = { 1, 0, 0, 0 };

                vkCmdClearColorImage(cmd, context.swapchainImages[swapchainIndex], VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1u, &clearSubresourceRange);
            });
    }
    catch (std::runtime_error& e)
    {
        std::cout << "Fatal: " << e.what() << std::endl;
    }

    return 0;
}
