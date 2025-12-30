/*
 * Copyright (c) 2025 John M. Parsaie
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
                       [&](uint32_t frameIndex)
                       {
                           auto& cmd        = context.frameCommandBuffer[frameIndex];
                           auto& backbuffer = context.swapchainImages[frameIndex];

                           // -----

                           {
                               barriersI[0].image                       = backbuffer;
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

                           vkCmdClearColorImage(cmd, backbuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1u, &clearSubresourceRange);

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
