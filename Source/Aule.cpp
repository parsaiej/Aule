#include "../Include/Aule/Aule.h"
#include "AulePrecompiled.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <GLFW/glfw3.h>

using namespace Aule;

inline void ThrowOnFail(VkResult result)
{
    if (result != VK_SUCCESS)
        throw std::runtime_error("Internal Vulkan call failed.");
}

// Implementation
// -----------------------

Context Aule::CreateContext(const Params& params)
{
    Context ctx = {};

    // ----------------------------------

    if (!glfwInit())
        exit(1);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    ctx.window = glfwCreateWindow(params.windowWidth, params.windowHeight, params.windowName, nullptr, nullptr);

    if (!ctx.window)
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

    ThrowOnFail(vkCreateInstance(&instanceInfo, nullptr, &ctx.instance));

    volkLoadInstance(ctx.instance);

    // ----------------------------------

    uint32_t physicalDeviceCount;
    ThrowOnFail(vkEnumeratePhysicalDevices(ctx.instance, &physicalDeviceCount, nullptr));

    if (physicalDeviceCount == 0)
        exit(1);

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    ThrowOnFail(vkEnumeratePhysicalDevices(ctx.instance, &physicalDeviceCount, physicalDevices.data()));

    // Select device #0 for now.
    ctx.devicePhysical = physicalDevices[0];

    vkGetPhysicalDeviceProperties(ctx.devicePhysical, &ctx.deviceProperties);

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.devicePhysical, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueInfos(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.devicePhysical, &queueFamilyCount, queueInfos.data());

    for (uint32_t queueInfoIndex = 0u; queueInfoIndex < queueFamilyCount; queueInfoIndex++)
    {
        if (!(queueInfos[queueInfoIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            continue;

        // Just grab first graphics compatible queue.
        ctx.queueIndex = queueInfoIndex;

        break;
    }

    // ----------------------------------

    VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };

    const float kGraphicsQueuePriority = 1.0f;

    queueCreateInfo.queueFamilyIndex = ctx.queueIndex;
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

    ThrowOnFail(vkCreateDevice(ctx.devicePhysical, &deviceInfo, nullptr, &ctx.deviceLogical));

    volkLoadDevice(ctx.deviceLogical);

    vkGetDeviceQueue(ctx.deviceLogical, ctx.queueIndex, 0u, &ctx.queue);

    // Surface
    // ---------------------

    ThrowOnFail(glfwCreateWindowSurface(ctx.instance, ctx.window, nullptr, &ctx.surface));

    uint32_t surfaceFormatCount;
    ThrowOnFail(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.devicePhysical, ctx.surface, &surfaceFormatCount, nullptr));

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    ThrowOnFail(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.devicePhysical, ctx.surface, &surfaceFormatCount, surfaceFormats.data()));
    ThrowOnFail(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.devicePhysical, ctx.surface, &ctx.surfaceInfo));

    VkSwapchainCreateInfoKHR swapChainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    {
        swapChainInfo.presentMode         = VK_PRESENT_MODE_FIFO_KHR;
        swapChainInfo.surface             = ctx.surface;
        swapChainInfo.minImageCount       = ctx.surfaceInfo.minImageCount;
        swapChainInfo.imageExtent         = ctx.surfaceInfo.currentExtent;
        swapChainInfo.preTransform        = ctx.surfaceInfo.currentTransform;
        swapChainInfo.pQueueFamilyIndices = &ctx.queueIndex;
        swapChainInfo.imageColorSpace     = surfaceFormats.at(0).colorSpace;
        swapChainInfo.imageFormat         = surfaceFormats.at(0).format;
        swapChainInfo.imageArrayLayers    = 1u;
        swapChainInfo.imageUsage          = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapChainInfo.compositeAlpha      = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    ThrowOnFail(vkCreateSwapchainKHR(ctx.deviceLogical, &swapChainInfo, nullptr, &ctx.swapchain));

    ctx.swapchainFormat = swapChainInfo.imageFormat;

    ThrowOnFail(vkGetSwapchainImagesKHR(ctx.deviceLogical, ctx.swapchain, &ctx.swapchainImageCount, nullptr));

    ctx.swapchainImages.resize(ctx.swapchainImageCount);
    ctx.swapchainImageViews.resize(ctx.swapchainImageCount);

    // ---------------------

    ThrowOnFail(vkGetSwapchainImagesKHR(ctx.deviceLogical, ctx.swapchain, &ctx.swapchainImageCount, ctx.swapchainImages.data()));

    for (uint32_t swapChainImageIndex = 0u; swapChainImageIndex < ctx.swapchainImageCount; swapChainImageIndex++)
    {
        VkImageViewCreateInfo imageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };

        imageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.image                           = ctx.swapchainImages[swapChainImageIndex];
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

        ThrowOnFail(vkCreateImageView(ctx.deviceLogical, &imageViewInfo, nullptr, &ctx.swapchainImageViews[swapChainImageIndex]));
    }

    // ---------------------

    ctx.frameCommandPool.resize(params.numFramesInFlight);
    ctx.frameCommandBuffer.resize(params.numFramesInFlight);
    ctx.frameSemaphoreImageAvailable.resize(params.numFramesInFlight);
    ctx.frameSemaphoreRenderComplete.resize(params.numFramesInFlight);
    ctx.frameFenceRenderComplete.resize(params.numFramesInFlight);

    for (uint32_t frameIndex = 0u; frameIndex < params.numFramesInFlight; frameIndex++)
    {
        VkSemaphoreCreateInfo sempahoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        ThrowOnFail(vkCreateSemaphore(ctx.deviceLogical, &sempahoreInfo, nullptr, &ctx.frameSemaphoreImageAvailable[frameIndex]));
        ThrowOnFail(vkCreateSemaphore(ctx.deviceLogical, &sempahoreInfo, nullptr, &ctx.frameSemaphoreRenderComplete[frameIndex]));

        VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
        ThrowOnFail(vkCreateFence(ctx.deviceLogical, &fenceInfo, nullptr, &ctx.frameFenceRenderComplete[frameIndex]));

        VkCommandPoolCreateInfo commandPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        {
            commandPoolInfo.queueFamilyIndex = ctx.queueIndex;
        }
        ThrowOnFail(vkCreateCommandPool(ctx.deviceLogical, &commandPoolInfo, nullptr, &ctx.frameCommandPool[frameIndex]));

        VkCommandBufferAllocateInfo commandAllocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        {
            commandAllocateInfo.commandBufferCount = 1u;
            commandAllocateInfo.commandPool        = ctx.frameCommandPool[frameIndex];
        }
        ThrowOnFail(vkAllocateCommandBuffers(ctx.deviceLogical, &commandAllocateInfo, &ctx.frameCommandBuffer[frameIndex]));
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
        allocatorInfo.instance         = ctx.instance;
        allocatorInfo.device           = ctx.deviceLogical;
        allocatorInfo.physicalDevice   = ctx.devicePhysical;
        allocatorInfo.pVulkanFunctions = &allocatorFunctions;
    }
    ThrowOnFail(vmaCreateAllocator(&allocatorInfo, &ctx.allocator));

    // -----------------------

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForVulkan(ctx.window, true);

    ImGui_ImplVulkan_InitInfo imguiInfo = {};
    {
        imguiInfo.Instance            = ctx.instance;
        imguiInfo.PhysicalDevice      = ctx.devicePhysical;
        imguiInfo.Device              = ctx.deviceLogical;
        imguiInfo.QueueFamily         = ctx.queueIndex;
        imguiInfo.Queue               = ctx.queue;
        imguiInfo.MinImageCount       = ctx.swapchainImageCount;
        imguiInfo.ImageCount          = ctx.swapchainImageCount;
        imguiInfo.UseDynamicRendering = true;
        imguiInfo.DescriptorPoolSize  = params.maxSupportedImguiImages;

        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount    = 1u;
        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &ctx.swapchainFormat;
    }

    ImGui_ImplVulkan_Init(&imguiInfo);

    // -----------------------

    return ctx;
}

void Aule::DestroyContext(Context& context)
{
    vkDeviceWaitIdle(context.deviceLogical);

    for (auto& swapchainImageView : context.swapchainImageViews)
        vkDestroyImageView(context.deviceLogical, swapchainImageView, nullptr);

    for (uint32_t frameIndex = 0u; frameIndex < context.frameCommandBuffer.size(); frameIndex++)
    {
        vkDestroyCommandPool(context.deviceLogical, context.frameCommandPool[frameIndex], nullptr);
        vkDestroySemaphore(context.deviceLogical, context.frameSemaphoreImageAvailable[frameIndex], nullptr);
        vkDestroySemaphore(context.deviceLogical, context.frameSemaphoreRenderComplete[frameIndex], nullptr);
        vkDestroyFence(context.deviceLogical, context.frameFenceRenderComplete[frameIndex], nullptr);
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    vkDestroySwapchainKHR(context.deviceLogical, context.swapchain, nullptr);
    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    vmaDestroyAllocator(context.allocator);
    vkDestroyDevice(context.deviceLogical, nullptr);
    vkDestroyInstance(context.instance, nullptr);

    glfwDestroyWindow(context.window);
}

void Aule::Dispatch(Context& ctx, std::function<void(uint32_t, uint32_t)> renderFrameCallback, std::mutex* pDispatchQueueMutex)
{
    uint32_t frameIndex = 0u;

    while (!glfwWindowShouldClose(ctx.window))
    {
        glfwPollEvents();

        // Pause thread until graphics queue finished processing.
        vkWaitForFences(ctx.deviceLogical, 1u, &ctx.frameFenceRenderComplete[frameIndex], VK_TRUE, UINT64_MAX);

        // Reset the fence for this frame.
        vkResetFences(ctx.deviceLogical, 1u, &ctx.frameFenceRenderComplete[frameIndex]);

        VkAcquireNextImageInfoKHR swapChainIndexAcquireInfo = { VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR };
        {
            swapChainIndexAcquireInfo.swapchain  = ctx.swapchain;
            swapChainIndexAcquireInfo.timeout    = UINT64_MAX;
            swapChainIndexAcquireInfo.semaphore  = ctx.frameSemaphoreImageAvailable[frameIndex];
            swapChainIndexAcquireInfo.deviceMask = 0x1;
        }

        uint32_t swapchainIndex;
        ThrowOnFail(vkAcquireNextImage2KHR(ctx.deviceLogical, &swapChainIndexAcquireInfo, &swapchainIndex));

        ThrowOnFail(vkResetCommandPool(ctx.deviceLogical, ctx.frameCommandPool[frameIndex], 0x0));

        VkCommandBufferBeginInfo cmdInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        ThrowOnFail(vkBeginCommandBuffer(ctx.frameCommandBuffer[frameIndex], &cmdInfo));

        // -----------------------

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // -----------------------

        renderFrameCallback(swapchainIndex, frameIndex);

        // -----------------------

        VkImageMemoryBarrier2 imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        {
            imageBarrier.image                       = ctx.swapchainImages[swapchainIndex];
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

        vkCmdPipelineBarrier2(ctx.frameCommandBuffer[frameIndex], &barriers);

        VkRenderingAttachmentInfo attachmentInfo = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        {
            attachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentInfo.loadOp      = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentInfo.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
            attachmentInfo.imageView   = ctx.swapchainImageViews[swapchainIndex];
        }

        VkRenderingInfo renderingInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
        {
            renderingInfo.colorAttachmentCount = 1u;
            renderingInfo.pColorAttachments    = &attachmentInfo;
            renderingInfo.layerCount           = 1u;
            renderingInfo.renderArea.extent    = ctx.surfaceInfo.currentExtent;
        }
        vkCmdBeginRendering(ctx.frameCommandBuffer[frameIndex], &renderingInfo);

        // If the user provided a mutex, lock it here and now (ImGui may do some internal queue submissions).
        if (pDispatchQueueMutex)
            std::lock_guard _(*pDispatchQueueMutex);

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ctx.frameCommandBuffer[frameIndex]);

        vkCmdEndRendering(ctx.frameCommandBuffer[frameIndex]);

        {
            imageBarrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageBarrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
            imageBarrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        }
        vkCmdPipelineBarrier2(ctx.frameCommandBuffer[frameIndex], &barriers);

        // -----------------------

        ThrowOnFail(vkEndCommandBuffer(ctx.frameCommandBuffer[frameIndex]));

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        {

            submitInfo.commandBufferCount   = 1u;
            submitInfo.pCommandBuffers      = &ctx.frameCommandBuffer[frameIndex];
            submitInfo.waitSemaphoreCount   = 1u;
            submitInfo.pWaitSemaphores      = &ctx.frameSemaphoreImageAvailable[frameIndex];
            submitInfo.pWaitDstStageMask    = &waitStage;
            submitInfo.signalSemaphoreCount = 1u;
            submitInfo.pSignalSemaphores    = &ctx.frameSemaphoreRenderComplete[frameIndex];
        }
        ThrowOnFail(vkQueueSubmit(ctx.queue, 1u, &submitInfo, ctx.frameFenceRenderComplete[frameIndex]));

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        {
            presentInfo.swapchainCount     = 1u;
            presentInfo.pSwapchains        = &ctx.swapchain;
            presentInfo.pImageIndices      = &swapchainIndex;
            presentInfo.waitSemaphoreCount = 1u;
            presentInfo.pWaitSemaphores    = &ctx.frameSemaphoreRenderComplete[frameIndex];
        }
        ThrowOnFail(vkQueuePresentKHR(ctx.queue, &presentInfo));

        // -----------------------

        frameIndex = (frameIndex + 1u) % ctx.frameCommandBuffer.size();
    }
}
