#include "../Include/Aule/Aule.h"

using namespace Aule;

inline void ThrowOnFail(VkResult result)
{
    if (result != VK_SUCCESS)
        throw std::runtime_error("Internal Vulkan call failed.");
}

// State
// -----------------------

Context g_Context = {};

// Implementation
// -----------------------

Context* Aule::Initialize(const Params& params)
{
    // ----------------------------------

    if (!glfwInit())
        exit(1);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    g_Context.window = glfwCreateWindow(params.windowWidth, params.windowHeight, params.windowName, nullptr, nullptr);

    if (!g_Context.window)
        exit(1);

    ThrowOnFail(volkInitialize());

    // ----------------------------------

    VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    {
        applicationInfo.pApplicationName   = params.windowName;
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName        = "Aule";
        applicationInfo.engineVersion      = VK_MAKE_VERSION(0, 0, 0);
        applicationInfo.apiVersion         = VK_API_VERSION_1_3;
    }

    uint32_t requiredExtensionsCountGLFW;
    auto     requiredExtensionsGLFW = glfwGetRequiredInstanceExtensions(&requiredExtensionsCountGLFW);

    VkInstanceCreateInfo instanceInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    {
        instanceInfo.pApplicationInfo        = &applicationInfo;
        instanceInfo.enabledExtensionCount   = requiredExtensionsCountGLFW;
        instanceInfo.ppEnabledExtensionNames = requiredExtensionsGLFW;
    }

    ThrowOnFail(vkCreateInstance(&instanceInfo, nullptr, &g_Context.instance));

    volkLoadInstance(g_Context.instance);

    // ----------------------------------

    uint32_t physicalDeviceCount;
    ThrowOnFail(vkEnumeratePhysicalDevices(g_Context.instance, &physicalDeviceCount, nullptr));

    if (physicalDeviceCount == 0)
        exit(1);

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    ThrowOnFail(vkEnumeratePhysicalDevices(g_Context.instance, &physicalDeviceCount, physicalDevices.data()));

    // Select device #0 for now.
    g_Context.devicePhysical = physicalDevices[0];

    vkGetPhysicalDeviceProperties(g_Context.devicePhysical, &g_Context.deviceProperties);

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(g_Context.devicePhysical, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueInfos(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(g_Context.devicePhysical, &queueFamilyCount, queueInfos.data());

    for (uint32_t queueInfoIndex = 0u; queueInfoIndex < queueFamilyCount; queueInfoIndex++)
    {
        if (!(queueInfos[queueInfoIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            continue;

        // Just grab first graphics compatible queue.
        g_Context.queueIndex = queueInfoIndex;

        break;
    }

    // ----------------------------------

    VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };

    const float kGraphicsQueuePriority = 1.0f;

    queueCreateInfo.queueFamilyIndex = g_Context.queueIndex;
    queueCreateInfo.queueCount       = 1u;
    queueCreateInfo.pQueuePriorities = &kGraphicsQueuePriority;

    VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };

    std::vector<const char*> deviceExtensions;
    {
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    }

    VkPhysicalDeviceSynchronization2Features deviceSyncFeature = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
    {
        deviceSyncFeature.synchronization2 = VK_TRUE;
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
    {
        dynamicRenderingFeature.pNext            = &deviceSyncFeature;
        dynamicRenderingFeature.dynamicRendering = VK_TRUE;
    }

    VkPhysicalDeviceFeatures2 deviceFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    {
        deviceFeatures.pNext = &dynamicRenderingFeature;
    }

    deviceInfo.pNext                   = &deviceFeatures;
    deviceInfo.queueCreateInfoCount    = 1u;
    deviceInfo.pQueueCreateInfos       = &queueCreateInfo;
    deviceInfo.enabledExtensionCount   = deviceExtensions.size();
    deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

    ThrowOnFail(vkCreateDevice(g_Context.devicePhysical, &deviceInfo, nullptr, &g_Context.deviceLogical));

    volkLoadDevice(g_Context.deviceLogical);

    vkGetDeviceQueue(g_Context.deviceLogical, g_Context.queueIndex, 0u, &g_Context.queue);

    // Surface
    // ---------------------

    ThrowOnFail(glfwCreateWindowSurface(g_Context.instance, g_Context.window, nullptr, &g_Context.surface));

    uint32_t surfaceFormatCount;
    ThrowOnFail(vkGetPhysicalDeviceSurfaceFormatsKHR(g_Context.devicePhysical, g_Context.surface, &surfaceFormatCount, nullptr));

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    ThrowOnFail(vkGetPhysicalDeviceSurfaceFormatsKHR(g_Context.devicePhysical, g_Context.surface, &surfaceFormatCount, surfaceFormats.data()));
    ThrowOnFail(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_Context.devicePhysical, g_Context.surface, &g_Context.surfaceInfo));

    VkSwapchainCreateInfoKHR swapChainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    {
        swapChainInfo.presentMode         = VK_PRESENT_MODE_FIFO_KHR;
        swapChainInfo.surface             = g_Context.surface;
        swapChainInfo.minImageCount       = g_Context.surfaceInfo.minImageCount;
        swapChainInfo.imageExtent         = g_Context.surfaceInfo.currentExtent;
        swapChainInfo.preTransform        = g_Context.surfaceInfo.currentTransform;
        swapChainInfo.pQueueFamilyIndices = &g_Context.queueIndex;
        swapChainInfo.imageColorSpace     = surfaceFormats.at(0).colorSpace;
        swapChainInfo.imageFormat         = surfaceFormats.at(0).format;
        swapChainInfo.imageArrayLayers    = 1u;
        swapChainInfo.imageUsage          = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapChainInfo.compositeAlpha      = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    ThrowOnFail(vkCreateSwapchainKHR(g_Context.deviceLogical, &swapChainInfo, nullptr, &g_Context.swapchain));

    g_Context.swapchainFormat = swapChainInfo.imageFormat;

    ThrowOnFail(vkGetSwapchainImagesKHR(g_Context.deviceLogical, g_Context.swapchain, &g_Context.swapchainImageCount, nullptr));

    g_Context.swapchainImages.resize(g_Context.swapchainImageCount);
    g_Context.swapchainImageViews.resize(g_Context.swapchainImageCount);

    // ---------------------

    ThrowOnFail(
        vkGetSwapchainImagesKHR(g_Context.deviceLogical, g_Context.swapchain, &g_Context.swapchainImageCount, g_Context.swapchainImages.data()));

    for (uint32_t swapChainImageIndex = 0u; swapChainImageIndex < g_Context.swapchainImageCount; swapChainImageIndex++)
    {
        VkImageViewCreateInfo imageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };

        imageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.image                           = g_Context.swapchainImages[swapChainImageIndex];
        imageViewInfo.format                          = swapChainInfo.imageFormat;
        imageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel   = 0u;
        imageViewInfo.subresourceRange.levelCount     = 1u;
        imageViewInfo.subresourceRange.baseArrayLayer = 0u;
        imageViewInfo.subresourceRange.layerCount     = 1u;
        imageViewInfo.components                      = { VK_COMPONENT_SWIZZLE_IDENTITY,
                                                          VK_COMPONENT_SWIZZLE_IDENTITY,
                                                          VK_COMPONENT_SWIZZLE_IDENTITY,
                                                          VK_COMPONENT_SWIZZLE_IDENTITY };

        ThrowOnFail(vkCreateImageView(g_Context.deviceLogical, &imageViewInfo, nullptr, &g_Context.swapchainImageViews[swapChainImageIndex]));
    }

    // ---------------------

    g_Context.frameCommandPool.resize(params.numFramesInFlight);
    g_Context.frameCommandBuffer.resize(params.numFramesInFlight);
    g_Context.frameSemaphoreImageAvailable.resize(params.numFramesInFlight);
    g_Context.frameSemaphoreRenderComplete.resize(params.numFramesInFlight);
    g_Context.frameFenceRenderComplete.resize(params.numFramesInFlight);

    for (uint32_t frameIndex = 0u; frameIndex < params.numFramesInFlight; frameIndex++)
    {
        VkSemaphoreCreateInfo sempahoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        ThrowOnFail(vkCreateSemaphore(g_Context.deviceLogical, &sempahoreInfo, nullptr, &g_Context.frameSemaphoreImageAvailable[frameIndex]));
        ThrowOnFail(vkCreateSemaphore(g_Context.deviceLogical, &sempahoreInfo, nullptr, &g_Context.frameSemaphoreRenderComplete[frameIndex]));

        VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
        ThrowOnFail(vkCreateFence(g_Context.deviceLogical, &fenceInfo, nullptr, &g_Context.frameFenceRenderComplete[frameIndex]));

        VkCommandPoolCreateInfo commandPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        {
            commandPoolInfo.queueFamilyIndex = g_Context.queueIndex;
        }
        ThrowOnFail(vkCreateCommandPool(g_Context.deviceLogical, &commandPoolInfo, nullptr, &g_Context.frameCommandPool[frameIndex]));

        VkCommandBufferAllocateInfo commandAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        {
            commandAllocateInfo.commandBufferCount = 1u;
            commandAllocateInfo.commandPool        = g_Context.frameCommandPool[frameIndex];
        }
        ThrowOnFail(vkAllocateCommandBuffers(g_Context.deviceLogical, &commandAllocateInfo, &g_Context.frameCommandBuffer[frameIndex]));
    }

    // Memory Allocator
    // ----------------------

    VmaVulkanFunctions allocatorFunctions = {};
    {
        allocatorFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        allocatorFunctions.vkGetDeviceProcAddr   = vkGetDeviceProcAddr;
    }

    VmaAllocatorCreateInfo allocatorInfo = {};
    {
        allocatorInfo.instance         = g_Context.instance;
        allocatorInfo.device           = g_Context.deviceLogical;
        allocatorInfo.physicalDevice   = g_Context.devicePhysical;
        allocatorInfo.pVulkanFunctions = &allocatorFunctions;
    }
    ThrowOnFail(vmaCreateAllocator(&allocatorInfo, &g_Context.allocator));

    // -----------------------

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForVulkan(g_Context.window, true);

    ImGui_ImplVulkan_InitInfo imguiInfo = {};
    {
        imguiInfo.Instance            = g_Context.instance;
        imguiInfo.PhysicalDevice      = g_Context.devicePhysical;
        imguiInfo.Device              = g_Context.deviceLogical;
        imguiInfo.QueueFamily         = g_Context.queueIndex;
        imguiInfo.Queue               = g_Context.queue;
        imguiInfo.MinImageCount       = g_Context.swapchainImageCount;
        imguiInfo.ImageCount          = g_Context.swapchainImageCount;
        imguiInfo.UseDynamicRendering = true;
        imguiInfo.DescriptorPoolSize  = params.maxSupportedImguiImages;

        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount    = 1u;
        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &g_Context.swapchainFormat;
    }

    ImGui_ImplVulkan_Init(&imguiInfo);

    // -----------------------

    return &g_Context;
}

void Aule::Dispatch(std::function<void(uint32_t, uint32_t)> renderFrameCallback)
{
    uint32_t frameIndex = 0u;

    while (!glfwWindowShouldClose(g_Context.window))
    {
        glfwPollEvents();

        // Pause thread until graphics queue finished processing.
        vkWaitForFences(g_Context.deviceLogical, 1u, &g_Context.frameFenceRenderComplete[frameIndex], VK_TRUE, UINT64_MAX);

        // Reset the fence for this frame.
        vkResetFences(g_Context.deviceLogical, 1u, &g_Context.frameFenceRenderComplete[frameIndex]);

        VkAcquireNextImageInfoKHR swapChainIndexAcquireInfo = { VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR };
        {
            swapChainIndexAcquireInfo.swapchain  = g_Context.swapchain;
            swapChainIndexAcquireInfo.timeout    = UINT64_MAX;
            swapChainIndexAcquireInfo.semaphore  = g_Context.frameSemaphoreImageAvailable[frameIndex];
            swapChainIndexAcquireInfo.deviceMask = 0x1;
        }

        uint32_t swapchainIndex;
        ThrowOnFail(vkAcquireNextImage2KHR(g_Context.deviceLogical, &swapChainIndexAcquireInfo, &swapchainIndex));

        ThrowOnFail(vkResetCommandPool(g_Context.deviceLogical, g_Context.frameCommandPool[frameIndex], 0x0));

        VkCommandBufferBeginInfo cmdInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        ThrowOnFail(vkBeginCommandBuffer(g_Context.frameCommandBuffer[frameIndex], &cmdInfo));

        // -----------------------

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // -----------------------

        renderFrameCallback(swapchainIndex, frameIndex);

        // -----------------------

        VkImageMemoryBarrier2 imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        {
            imageBarrier.image                       = g_Context.swapchainImages[swapchainIndex];
            imageBarrier.oldLayout                   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imageBarrier.newLayout                   = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageBarrier.srcAccessMask               = VK_ACCESS_2_MEMORY_READ_BIT;
            imageBarrier.dstAccessMask               = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            imageBarrier.srcStageMask                = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            imageBarrier.dstStageMask                = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBarrier.subresourceRange.layerCount = 1u;
            imageBarrier.subresourceRange.levelCount = 1u;
        }

        VkDependencyInfo barriers = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        {
            barriers.imageMemoryBarrierCount = 1u;
            barriers.pImageMemoryBarriers    = &imageBarrier;
        }

        vkCmdPipelineBarrier2(g_Context.frameCommandBuffer[frameIndex], &barriers);

        VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        {
            attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentInfo.loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentInfo.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
            attachmentInfo.imageView   = g_Context.swapchainImageViews[swapchainIndex];
        }

        VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
        {
            renderingInfo.colorAttachmentCount = 1u;
            renderingInfo.pColorAttachments    = &attachmentInfo;
            renderingInfo.layerCount           = 1u;
            renderingInfo.renderArea.extent    = g_Context.surfaceInfo.currentExtent;
        }
        vkCmdBeginRendering(g_Context.frameCommandBuffer[frameIndex], &renderingInfo);

        // Lock here since imgui may submit to it...
        std::lock_guard _(g_Context.queueMutex);

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), g_Context.frameCommandBuffer[frameIndex]);

        vkCmdEndRendering(g_Context.frameCommandBuffer[frameIndex]);

        {
            imageBarrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageBarrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
            imageBarrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        }
        vkCmdPipelineBarrier2(g_Context.frameCommandBuffer[frameIndex], &barriers);

        // -----------------------

        ThrowOnFail(vkEndCommandBuffer(g_Context.frameCommandBuffer[frameIndex]));

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        {

            submitInfo.commandBufferCount   = 1u;
            submitInfo.pCommandBuffers      = &g_Context.frameCommandBuffer[frameIndex];
            submitInfo.waitSemaphoreCount   = 1u;
            submitInfo.pWaitSemaphores      = &g_Context.frameSemaphoreImageAvailable[frameIndex];
            submitInfo.pWaitDstStageMask    = &waitStage;
            submitInfo.signalSemaphoreCount = 1u;
            submitInfo.pSignalSemaphores    = &g_Context.frameSemaphoreRenderComplete[frameIndex];
        }
        ThrowOnFail(vkQueueSubmit(g_Context.queue, 1u, &submitInfo, g_Context.frameFenceRenderComplete[frameIndex]));

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        {
            presentInfo.swapchainCount     = 1u;
            presentInfo.pSwapchains        = &g_Context.swapchain;
            presentInfo.pImageIndices      = &swapchainIndex;
            presentInfo.waitSemaphoreCount = 1u;
            presentInfo.pWaitSemaphores    = &g_Context.frameSemaphoreRenderComplete[frameIndex];
        }
        ThrowOnFail(vkQueuePresentKHR(g_Context.queue, &presentInfo));

        // -----------------------

        frameIndex = (frameIndex + 1u) % g_Context.frameCommandBuffer.size();
    }
}
