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
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
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
    struct Params
    {
        // Basic operating system window information.
        const char* windowName;
        uint32_t    windowWidth;
        uint32_t    windowHeight;

        // Try to initialize with a device that contains this string in its
        // description.
        const char* deviceHint = nullptr;

        // Attempt to load this list of Vulkan extensions. Log a warning if
        // extension is not found in driver.
        std::vector<const char*> deviceExtensions;

        // Due to how ImGui Vulkan images work we need to specify descriptor
        // pool size.
        uint32_t maxSupportedImguiImages = 512u;

        // Number of CPU-side render iterations the application is allowed to
        // have in flight on the GPU concurrently. Independent of swapchain
        // image count. 2 is a sensible default; 3 reduces CPU stalls at the
        // cost of more memory and one frame of input latency.
        uint32_t framesInFlight = 2u;
    };

    struct Context
    {
        // Cross-platform operating system window context.
        GLFWwindow* window;

        // Core Vulkan instance objects.
        VkInstance instance;
        VkDevice   device;

        // A physical device will be selected either by the provided hint or the
        // first device enumerated by vulkan loader.
        VkPhysicalDevice           selectedPhysicalDevice;
        VkPhysicalDeviceProperties selectedPhysicalDeviceProperties;

        // Enumeration of all queues for the supported device. The logical
        // device will be initialized to use 1 queue from each of the queue
        // families.
        uint32_t                              queueFamilyCount;
        std::vector<VkQueueFamilyProperties>  queueFamilyProperties;
        std::unordered_map<uint32_t, VkQueue> queues;

        // Context will use the first queue family that supports graphics for
        // imgui, graphics commands, swapchain present. Commands that get
        // recorded in the render lambda will be submitted to this queue.
        uint32_t selectedQueueFamilyIndex;

        // Basic memory allocator that can be used for allocations in your
        // application.
        VmaAllocator allocator;

        // Swapchain information.
        VkSurfaceKHR             surface;
        VkSurfaceCapabilitiesKHR surfaceInfo;
        VkSwapchainKHR           swapchain;

        // ----- Per swapchain image (indexed by swapchainIndex) -----
        // The driver decides this count. Resources here are tied to specific
        // swapchain images; the render-complete semaphore must be per-image
        // because vkQueuePresentKHR waits on it against a specific image.
        uint32_t                 swapchainImageCount;
        std::vector<VkImage>     swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        std::vector<VkSemaphore> swapchainSemaphoreRenderComplete;

        // ----- Per frame-in-flight (indexed by frameInFlightIndex) -----
        // Application-chosen via Params::framesInFlight. Resources here gate
        // CPU/GPU overlap and are independent of how many swapchain images
        // the driver hands out.
        uint32_t                                       framesInFlight;
        std::vector<VkCommandPool>                     frameCommandPool;
        std::vector<VkCommandBuffer>                   frameCommandBuffer;
        std::vector<VkSemaphore>                       frameSemaphoreImageAvailable;
        std::vector<VkFence>                           frameFenceRenderComplete;
        std::vector<std::deque<std::function<void()>>> frameDeletionQueues;
    };

    // Create's an operating system window and Vulkan runtime, with a linking
    // swapchain Return the context to user for them to create
    // application-specific things. Return mutable context due to the queue
    // mutex.
    Context CreateContext(const Params& params);

    // Destroy provided operating system window and Vulkan runtime.
    void DestroyContext(Context& context);

    // Dispatch a renderloop handling swapchain, frames in flight, basic
    // synchronization. and call back the user render function to fill out
    // commands for current frame. Callback MUST transfer the current swapchain
    // image to PRESENT.
    //
    // The callback receives two indices:
    //   frameInFlightIndex - use to index per-frame-in-flight resources
    //                        (frameCommandBuffer, frameDeletionQueues, and
    //                        any application-owned per-frame ring buffers).
    //   swapchainIndex     - use to index per-swapchain-image resources
    //                        (swapchainImages, swapchainImageViews).
    void Dispatch(
        Context&                                                              context,
        std::function<void(uint32_t frameInFlightIndex, uint32_t swapchainIndex)> renderFrameCallback,
        std::mutex*                                                           pDispatchQueueMutex = nullptr);

} // namespace Aule
