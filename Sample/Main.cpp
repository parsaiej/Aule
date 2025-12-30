#include "../Include/Aule/Aule.h"

#include <stdexcept>
#include <iostream>

int main(int argc, char** argv)
{
    Aule::Params params = {};
    {
        params.windowName   = "Aule Sample";
        params.windowWidth  = 1280u;
        params.windowHeight = 720u;
    }

    try
    {
        auto context = Aule::CreateContext(params);

        // Keep some barries laying around.
        std::array<VkImageMemoryBarrier2, 4u>  barriersI({ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 });
        std::array<VkBufferMemoryBarrier2, 4u> barriersB({ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 });

        VkDependencyInfo barriers = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

        Aule::Dispatch(context,
                       [&](uint32_t swapchainIndex, uint32_t frameIndex)
                       {
                           auto& cmd = context.frameCommandBuffer[frameIndex];

                           // -----

                           {
                               barriersI[0].image                       = context.swapchainImages[swapchainIndex];
                               barriersI[0].oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
                               barriersI[0].newLayout                   = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                               barriersI[0].srcAccessMask               = VK_ACCESS_2_NONE;
                               barriersI[0].dstAccessMask               = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                               barriersI[0].srcStageMask                = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                               barriersI[0].dstStageMask                = VK_PIPELINE_STAGE_2_CLEAR_BIT;
                               barriersI[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                               barriersI[0].subresourceRange.layerCount = 1u;
                               barriersI[0].subresourceRange.levelCount = 1u;

                               barriers.pImageMemoryBarriers    = &barriersI[0];
                               barriers.imageMemoryBarrierCount = 1u;
                           }
                           vkCmdPipelineBarrier2(cmd, &barriers);

                           // -----

                           VkImageSubresourceRange clearSubresourceRange = {};
                           {
                               clearSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                               clearSubresourceRange.layerCount = 1u;
                               clearSubresourceRange.levelCount = 1u;
                           }

                           VkClearColorValue clearColor = { 1, 0, 0, 0 };

                           vkCmdClearColorImage(cmd,
                                                context.swapchainImages[swapchainIndex],
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                &clearColor,
                                                1u,
                                                &clearSubresourceRange);

                           // -----

                           {
                               barriersI[0].oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                               barriersI[0].newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                               barriersI[0].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                               barriersI[0].dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
                               barriersI[0].srcStageMask  = VK_PIPELINE_STAGE_2_CLEAR_BIT;
                               barriersI[0].dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
                           }
                           vkCmdPipelineBarrier2(cmd, &barriers);

                           // -----

                           ImGui::ShowDemoWindow();
                       });

        Aule::DestroyContext(context);
    }
    catch (std::runtime_error& e)
    {
        std::cout << "Fatal: " << e.what() << std::endl;
    }

    return 0;
}
