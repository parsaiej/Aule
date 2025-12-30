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

namespace Aule
{
    struct Context
    {
        GLFWwindow*                window;
        uint64_t                   frameCount;
        VkInstance                 instance;
        VkPhysicalDevice           devicePhysical;
        VkDevice                   deviceLogical;
        VkPhysicalDeviceProperties deviceProperties;
        VmaAllocator               allocator;
        uint32_t                   queueIndex;
        VkQueue                    queue;
        VkSurfaceKHR               surface;
        VkSurfaceCapabilitiesKHR   surfaceInfo;
        VkSwapchainKHR             swapchain;
        uint32_t                   swapchainImageCount;
        VkFormat                   swapchainFormat;

        // Index with the `frameIndex` passed by the Dispatch callback.
        std::vector<VkImage>         swapchainImages;
        std::vector<VkImageView>     swapchainImageViews;
        std::vector<VkCommandPool>   frameCommandPool;
        std::vector<VkCommandBuffer> frameCommandBuffer;
        std::vector<VkSemaphore>     frameSemaphoreImageAvailable;
        std::vector<VkSemaphore>     frameSemaphoreRenderComplete;
        std::vector<VkFence>         frameFenceRenderComplete;
    };

    struct Params
    {
        // Basic operating system window information.
        const char* windowName;
        uint32_t    windowWidth;
        uint32_t    windowHeight;

        // Due to how ImGui Vulkan images work we need to specify descriptor pool size.
        uint32_t maxSupportedImguiImages = 512u;
    };

    // Create's an operating system window and Vulkan runtime, with a linking swapchain
    // Return the context to user for them to create application-specific things.
    // Return mutable context due to the queue mutex.
    Context CreateContext(const Params& params);

    // Destroy provided operating system window and Vulkan runtime.
    void DestroyContext(Context& context);

    // Dispatch a renderloop handling swapchain, frames in flight, basic synchronization.
    // and call back the user render function to fill out commands for current frame. Callback MUST
    // transfer the current swapchain image to PRESENT.
    void Dispatch(Context& context, std::function<void(uint32_t frameIndex)> renderFrameCallback, std::mutex* pDispatchQueueMutex = nullptr);

} // namespace Aule
