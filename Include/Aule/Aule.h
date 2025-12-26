namespace Aule
{
    struct DeviceContext
    {
        VkInstance       vkInstance;
        VkPhysicalDevice vkPhysicalDevice;
        VkDevice         vkLogicalDevice;
        VmaAllocator     vkAllocator;
    };

    struct FrameContext
    {
        VkCommandBuffer vkCmd;
    };

    struct Params
    {
        const char*                        windowName;
        uint32_t                           windowWidth;
        uint32_t                           windowHeight;
        std::function<void()>              setupCallback;
        std::function<void(FrameContext&)> renderCallback;
    };

    void Go(const Params& params);

} // namespace Aule
