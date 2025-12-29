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
        std::mutex                 queueMutex;
        VkQueue                    queue;
        VkSurfaceKHR               surface;
        VkSurfaceCapabilitiesKHR   surfaceInfo;
        VkSwapchainKHR             swapchain;
        uint32_t                   swapchainImageCount;
        VkFormat                   swapchainFormat;

        // Index with the `swapchainIndex` passed by the Dispatch callback.
        std::vector<VkImage>     swapchainImages;
        std::vector<VkImageView> swapchainImageViews;

        // Index with the `frameIndex` passed by the Dispatch callback.
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

        // Specify how many frames can be queued by the swapchain.
        uint32_t numFramesInFlight = 2u;

        // Due to how ImGui Vulkan images work we need to specify descriptor pool size.
        uint32_t maxSupportedImguiImages = 512u;
    };

    // Create's an operating system window and Vulkan runtime, with a linking swapchain
    // Return the context to user for them to create application-specific things.
    // Return mutable context due to the queue mutex.
    Context* Initialize(const Params& params);

    // Dispatch a renderloop handling swapchain, frames in flight, basic synchronization.
    // and call back the user render function to fill out commands for current frame. Callback MUST
    // transfer the current swapchain image to PRESENT.
    void Dispatch(std::function<void(uint32_t swapchainIndex, uint32_t frameIndex)> renderFrameCallback);

} // namespace Aule
