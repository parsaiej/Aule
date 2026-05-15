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

#include "../Include/Aule/Aule.h"

using namespace Aule;

inline void ThrowOnFail(bool succeeded)
{
    if (!succeeded)
        throw std::runtime_error("Internal Vulkan call failed.");
}

inline void ThrowOnFail(VkResult result) { ThrowOnFail(result == VK_SUCCESS); }

// Implementation
// -----------------------

Context Aule::CreateContext(const Params& params)
{
    Context ctx = {};

    assert(params.windowName != nullptr);
    assert(params.windowWidth != 0);
    assert(params.windowHeight != 0);

    // ----------------------------------

#ifdef __linux__
    // X11 is better than Wayland in this case due to better RADV tracing support.
    // And also a weird bug in imgui scaling that I am too lazy to fix at the moment.
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

    ThrowOnFail(glfwInit());

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    ctx.window = glfwCreateWindow(params.windowWidth,
                                  params.windowHeight,
                                  params.windowName,
                                  nullptr,
                                  nullptr);

    ThrowOnFail(ctx.window);

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
    auto requiredExtensionsGLFW = glfwGetRequiredInstanceExtensions(&requiredExtensionsCountGLFW);

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

    // No drivers found!
    ThrowOnFail(physicalDeviceCount > 0);

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    ThrowOnFail(
        vkEnumeratePhysicalDevices(ctx.instance, &physicalDeviceCount, physicalDevices.data()));

    ctx.selectedPhysicalDevice = VK_NULL_HANDLE;

    if (params.deviceHint != nullptr)
    {
        for (auto& device : physicalDevices)
        {
            VkPhysicalDeviceProperties deviceInfo;
            vkGetPhysicalDeviceProperties(device, &deviceInfo);

            if (!strstr(deviceInfo.deviceName, params.deviceHint))
                continue;

            ctx.selectedPhysicalDevice = device;

            break;
        }
    }

    // Default to device zero if no hint is provided or does not match.
    if (ctx.selectedPhysicalDevice == VK_NULL_HANDLE)
        ctx.selectedPhysicalDevice = physicalDevices[0];

    // Store the properties for the user.
    vkGetPhysicalDeviceProperties(ctx.selectedPhysicalDevice,
                                  &ctx.selectedPhysicalDeviceProperties);

    vkGetPhysicalDeviceQueueFamilyProperties(ctx.selectedPhysicalDevice,
                                             &ctx.queueFamilyCount,
                                             nullptr);

    ctx.queueFamilyProperties.resize(ctx.queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.selectedPhysicalDevice,
                                             &ctx.queueFamilyCount,
                                             ctx.queueFamilyProperties.data());

    for (uint32_t queueFamilyIndex = 0u; queueFamilyIndex < ctx.queueFamilyCount;
         queueFamilyIndex++)
    {
        if (!(ctx.queueFamilyProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            continue;

        // Just grab first graphics compatible queue.
        ctx.selectedQueueFamilyIndex = queueFamilyIndex;
    }

    // ----------------------------------

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(
        ctx.queueFamilyCount,
        { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO });

    // Currently we make no effort to prioritize one over the other...
    const float kQueuePriority = 1.0f;

    for (uint32_t queueFamilyIndex = 0u; queueFamilyIndex < ctx.queueFamilyCount;
         queueFamilyIndex++)
    {
        // ONE queue will be created for each family.
        queueCreateInfos[queueFamilyIndex].queueFamilyIndex = queueFamilyIndex;
        queueCreateInfos[queueFamilyIndex].queueCount       = 1u;
        queueCreateInfos[queueFamilyIndex].pQueuePriorities = &kQueuePriority;
    }

    VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };

    std::vector<const char*> extensions;
    {
        extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);

        // Emplace user-requested extenstions.
        extensions.insert(extensions.end(),
                          params.deviceExtensions.begin(),
                          params.deviceExtensions.end());
    }

    // First check.
    {
        uint32_t supportedDeviceExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(ctx.selectedPhysicalDevice,
                                             nullptr,
                                             &supportedDeviceExtensionCount,
                                             nullptr);

        std::vector<VkExtensionProperties> supportedDeviceExtensions(supportedDeviceExtensionCount);
        vkEnumerateDeviceExtensionProperties(ctx.selectedPhysicalDevice,
                                             nullptr,
                                             &supportedDeviceExtensionCount,
                                             supportedDeviceExtensions.data());

        auto DeviceExtensionSupported = [&](const char* extension)
        {
            for (const auto& supportedExtension : supportedDeviceExtensions)
            {
                if (strcmp(supportedExtension.extensionName, extension) == 0)
                    return true;
            }

            return false;
        };

        for (const auto& requestedExtension : extensions)
            ThrowOnFail(DeviceExtensionSupported(requestedExtension));
    }

    VkPhysicalDeviceFeatures2                features                = {};
    VkPhysicalDeviceSynchronization2Features featureSync2            = {};
    VkPhysicalDeviceDynamicRenderingFeatures featureDynamicRendering = {};

    features.sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    featureSync2.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    featureDynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;

    features.pNext     = &featureSync2;
    featureSync2.pNext = &featureDynamicRendering;

    featureSync2.synchronization2            = VK_TRUE;
    featureDynamicRendering.dynamicRendering = VK_TRUE;

    deviceInfo.pNext                   = &features;
    deviceInfo.queueCreateInfoCount    = queueCreateInfos.size();
    deviceInfo.pQueueCreateInfos       = queueCreateInfos.data();
    deviceInfo.enabledExtensionCount   = extensions.size();
    deviceInfo.ppEnabledExtensionNames = extensions.data();

    ThrowOnFail(vkCreateDevice(ctx.selectedPhysicalDevice, &deviceInfo, nullptr, &ctx.device));

    volkLoadDevice(ctx.device);

    for (uint32_t queueFamilyIndex = 0u; queueFamilyIndex < ctx.queueFamilyCount;
         queueFamilyIndex++)
    {
        // Load queues into the map
        vkGetDeviceQueue(ctx.device, queueFamilyIndex, 0u, &ctx.queues[queueFamilyIndex]);
    }

    // Surface
    // ---------------------

    ThrowOnFail(glfwCreateWindowSurface(ctx.instance, ctx.window, nullptr, &ctx.surface));

    uint32_t surfaceFormatCount;
    ThrowOnFail(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.selectedPhysicalDevice,
                                                     ctx.surface,
                                                     &surfaceFormatCount,
                                                     nullptr));

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    ThrowOnFail(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.selectedPhysicalDevice,
                                                     ctx.surface,
                                                     &surfaceFormatCount,
                                                     surfaceFormats.data()));
    ThrowOnFail(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.selectedPhysicalDevice,
                                                          ctx.surface,
                                                          &ctx.surfaceInfo));

    VkSwapchainCreateInfoKHR swapChainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };

#if defined(__linux__)
    ctx.surfaceInfo.currentExtent.width  = params.windowWidth;
    ctx.surfaceInfo.currentExtent.height = params.windowHeight;
#endif

    {
        swapChainInfo.presentMode         = VK_PRESENT_MODE_FIFO_KHR;
        swapChainInfo.surface             = ctx.surface;
        swapChainInfo.minImageCount       = ctx.surfaceInfo.minImageCount;
        swapChainInfo.imageExtent         = ctx.surfaceInfo.currentExtent;
        swapChainInfo.preTransform        = ctx.surfaceInfo.currentTransform;
        swapChainInfo.pQueueFamilyIndices = &ctx.selectedQueueFamilyIndex;
        swapChainInfo.imageColorSpace     = surfaceFormats.at(0).colorSpace;
        swapChainInfo.imageFormat         = surfaceFormats.at(0).format;
        swapChainInfo.imageArrayLayers    = 1u;
        swapChainInfo.imageUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }

    ThrowOnFail(vkCreateSwapchainKHR(ctx.device, &swapChainInfo, nullptr, &ctx.swapchain));

    ThrowOnFail(
        vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain, &ctx.swapchainImageCount, nullptr));

    // ---------------------
    // Per-swapchain-image resources (sized by driver-decided image count).
    // ---------------------

    ctx.swapchainImages.resize(ctx.swapchainImageCount);
    ctx.swapchainImageViews.resize(ctx.swapchainImageCount);
    ctx.swapchainSemaphoreRenderComplete.resize(ctx.swapchainImageCount);

    ThrowOnFail(vkGetSwapchainImagesKHR(ctx.device,
                                        ctx.swapchain,
                                        &ctx.swapchainImageCount,
                                        ctx.swapchainImages.data()));

    for (uint32_t imageIndex = 0u; imageIndex < ctx.swapchainImageCount; imageIndex++)
    {
        VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        ThrowOnFail(vkCreateSemaphore(ctx.device,
                                      &semaphoreInfo,
                                      nullptr,
                                      &ctx.swapchainSemaphoreRenderComplete[imageIndex]));

        VkImageViewCreateInfo imageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };

        imageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.image                           = ctx.swapchainImages[imageIndex];
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

        ThrowOnFail(vkCreateImageView(ctx.device,
                                      &imageViewInfo,
                                      nullptr,
                                      &ctx.swapchainImageViews[imageIndex]));
    }

    // ---------------------
    // Per-frame-in-flight resources (sized by application choice).
    // ---------------------

    ctx.framesInFlight = params.framesInFlight;
    assert(ctx.framesInFlight > 0u);

    ctx.frameCommandPool.resize(ctx.framesInFlight);
    ctx.frameCommandBuffer.resize(ctx.framesInFlight);
    ctx.frameSemaphoreImageAvailable.resize(ctx.framesInFlight);
    ctx.frameFenceRenderComplete.resize(ctx.framesInFlight);
    ctx.frameDeletionQueues.resize(ctx.framesInFlight);

    for (uint32_t frameInFlightIndex = 0u; frameInFlightIndex < ctx.framesInFlight;
         frameInFlightIndex++)
    {
        VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        ThrowOnFail(vkCreateSemaphore(ctx.device,
                                      &semaphoreInfo,
                                      nullptr,
                                      &ctx.frameSemaphoreImageAvailable[frameInFlightIndex]));

        VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
        ThrowOnFail(vkCreateFence(ctx.device,
                                  &fenceInfo,
                                  nullptr,
                                  &ctx.frameFenceRenderComplete[frameInFlightIndex]));

        VkCommandPoolCreateInfo commandPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        commandPoolInfo.queueFamilyIndex        = ctx.selectedQueueFamilyIndex;
        ThrowOnFail(vkCreateCommandPool(ctx.device,
                                        &commandPoolInfo,
                                        nullptr,
                                        &ctx.frameCommandPool[frameInFlightIndex]));

        VkCommandBufferAllocateInfo commandAllocateInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
        };
        commandAllocateInfo.commandBufferCount = 1u;
        commandAllocateInfo.commandPool        = ctx.frameCommandPool[frameInFlightIndex];
        ThrowOnFail(vkAllocateCommandBuffers(ctx.device,
                                             &commandAllocateInfo,
                                             &ctx.frameCommandBuffer[frameInFlightIndex]));
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
        allocatorInfo.device           = ctx.device;
        allocatorInfo.physicalDevice   = ctx.selectedPhysicalDevice;
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
        imguiInfo.PhysicalDevice      = ctx.selectedPhysicalDevice;
        imguiInfo.Device              = ctx.device;
        imguiInfo.QueueFamily         = ctx.selectedQueueFamilyIndex;
        imguiInfo.Queue               = ctx.queues[ctx.selectedQueueFamilyIndex];
        imguiInfo.MinImageCount       = ctx.swapchainImageCount;
        imguiInfo.ImageCount          = ctx.swapchainImageCount;
        imguiInfo.UseDynamicRendering = true;
        imguiInfo.DescriptorPoolSize  = params.maxSupportedImguiImages;

        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1u;
        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats =
            &swapChainInfo.imageFormat;
    }

    ImGui_ImplVulkan_Init(&imguiInfo);

    // -----------------------

    return ctx;
}

void Aule::DestroyContext(Context& context)
{
    vkDeviceWaitIdle(context.device);

    // Per-swapchain-image resources.
    for (auto& view : context.swapchainImageViews)
        vkDestroyImageView(context.device, view, nullptr);

    for (auto& sem : context.swapchainSemaphoreRenderComplete)
        vkDestroySemaphore(context.device, sem, nullptr);

    // Per-frame-in-flight resources.
    for (uint32_t i = 0u; i < context.framesInFlight; i++)
    {
        vkDestroyCommandPool(context.device, context.frameCommandPool[i], nullptr);
        vkDestroySemaphore(context.device, context.frameSemaphoreImageAvailable[i], nullptr);
        vkDestroyFence(context.device, context.frameFenceRenderComplete[i], nullptr);
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    vkDestroySwapchainKHR(context.device, context.swapchain, nullptr);
    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    vmaDestroyAllocator(context.allocator);
    vkDestroyDevice(context.device, nullptr);
    vkDestroyInstance(context.instance, nullptr);

    glfwDestroyWindow(context.window);
}

void Aule::Dispatch(Context&                                ctx,
                    std::function<void(uint32_t, uint32_t)> renderFrameCallback,
                    std::mutex*                             pDispatchQueueMutex)
{
    uint32_t frameInFlightIndex = 0u;

    while (!glfwWindowShouldClose(ctx.window))
    {
        glfwPollEvents();

        // Pause thread until graphics queue finished processing this frame-in-flight slot.
        vkWaitForFences(ctx.device,
                        1u,
                        &ctx.frameFenceRenderComplete[frameInFlightIndex],
                        VK_TRUE,
                        UINT64_MAX);

        // Process deletion queue for this frame-in-flight slot.
        {
            auto& frameDeletionQueue = ctx.frameDeletionQueues[frameInFlightIndex];

            while (!frameDeletionQueue.empty())
            {
                // Execute the stored lambda (e.g., vkDestroyBuffer)
                frameDeletionQueue.front()();

                frameDeletionQueue.pop_front();
            }
        }

        // Reset the fence for this frame-in-flight slot.
        vkResetFences(ctx.device, 1u, &ctx.frameFenceRenderComplete[frameInFlightIndex]);

        VkAcquireNextImageInfoKHR swapChainIndexAcquireInfo = {
            VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR
        };
        {
            swapChainIndexAcquireInfo.swapchain = ctx.swapchain;
            swapChainIndexAcquireInfo.timeout   = UINT64_MAX;
            swapChainIndexAcquireInfo.semaphore =
                ctx.frameSemaphoreImageAvailable[frameInFlightIndex];
            swapChainIndexAcquireInfo.deviceMask = 0x1;
        }

        uint32_t swapchainIndex;
        ThrowOnFail(
            vkAcquireNextImage2KHR(ctx.device, &swapChainIndexAcquireInfo, &swapchainIndex));

        auto& cmd = ctx.frameCommandBuffer[frameInFlightIndex];

        ThrowOnFail(vkResetCommandPool(ctx.device, ctx.frameCommandPool[frameInFlightIndex], 0x0));

        VkCommandBufferBeginInfo cmdInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        ThrowOnFail(vkBeginCommandBuffer(cmd, &cmdInfo));

        // -----------------------

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // -----------------------

        renderFrameCallback(frameInFlightIndex, swapchainIndex);

        // -----------------------

        VkImageMemoryBarrier2 imageBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        {
            imageBarrier.image         = ctx.swapchainImages[swapchainIndex];
            imageBarrier.oldLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imageBarrier.newLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            imageBarrier.srcStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            imageBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBarrier.subresourceRange.layerCount = 1u;
            imageBarrier.subresourceRange.levelCount = 1u;
        }

        VkDependencyInfo barriers = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        {
            barriers.imageMemoryBarrierCount = 1u;
            barriers.pImageMemoryBarriers    = &imageBarrier;
        }

        vkCmdPipelineBarrier2(cmd, &barriers);

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
        vkCmdBeginRendering(cmd, &renderingInfo);

        // If the user provided a mutex, lock it here and now (ImGui may do some
        // internal queue submissions).
        if (pDispatchQueueMutex)
            std::lock_guard _(*pDispatchQueueMutex);

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

        vkCmdEndRendering(cmd);

        {
            imageBarrier.oldLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageBarrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
            imageBarrier.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            imageBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        }
        vkCmdPipelineBarrier2(cmd, &barriers);

        // -----------------------

        ThrowOnFail(vkEndCommandBuffer(cmd));

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        {
            submitInfo.commandBufferCount   = 1u;
            submitInfo.pCommandBuffers      = &cmd;
            submitInfo.waitSemaphoreCount   = 1u;
            submitInfo.pWaitSemaphores      = &ctx.frameSemaphoreImageAvailable[frameInFlightIndex];
            submitInfo.pWaitDstStageMask    = &waitStage;
            submitInfo.signalSemaphoreCount = 1u;
            // Indexed by swapchainIndex: this semaphore is waited on by
            // vkQueuePresentKHR against a specific swapchain image, so its
            // lifetime is tied to the image, not the frame-in-flight slot.
            submitInfo.pSignalSemaphores = &ctx.swapchainSemaphoreRenderComplete[swapchainIndex];
        }
        ThrowOnFail(vkQueueSubmit(ctx.queues[ctx.selectedQueueFamilyIndex],
                                  1u,
                                  &submitInfo,
                                  ctx.frameFenceRenderComplete[frameInFlightIndex]));

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        {
            presentInfo.swapchainCount     = 1u;
            presentInfo.pSwapchains        = &ctx.swapchain;
            presentInfo.pImageIndices      = &swapchainIndex;
            presentInfo.waitSemaphoreCount = 1u;
            presentInfo.pWaitSemaphores    = &ctx.swapchainSemaphoreRenderComplete[swapchainIndex];
        }
        ThrowOnFail(vkQueuePresentKHR(ctx.queues[ctx.selectedQueueFamilyIndex], &presentInfo));

        // -----------------------

        frameInFlightIndex = (frameInFlightIndex + 1u) % ctx.framesInFlight;
    }
}
