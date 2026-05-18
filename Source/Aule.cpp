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

// Throws std::runtime_error on failure. Each call site supplies a short
// `context` string describing what was being attempted, which is embedded
// in the exception message to make failures actionable.
//
// The VkResult overload stringifies the result code via the Vulkan SDK's
// string_VkResult() helper from <vulkan/vk_enum_string_helper.h>. The bool
// overload is for non-VkResult checks (non-null handles, count > 0,
// extension availability, etc.).
inline void ThrowOnFail(VkResult result, const char* context)
{
    if (result == VK_SUCCESS)
        return;

    throw std::runtime_error(std::string("[Aule] ") + context +
                             " failed: " + string_VkResult(result));
}

inline void ThrowOnFail(bool succeeded, const char* context)
{
    if (!succeeded)
        throw std::runtime_error(std::string("[Aule] ") + context);
}

// -----------------------
// Swapchain lifecycle helpers
//
// These manage all per-swapchain-image resources together. They are called
// from CreateContext / DestroyContext, and from RecreateSwapchain on resize.
// They assume the device, surface, and selectedSurfaceFormat are already set.
// -----------------------

static void CreateSwapchainAndPerImageResources(Context& ctx)
{
    ThrowOnFail(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.selectedPhysicalDevice,
                                                          ctx.surface,
                                                          &ctx.surfaceInfo),
                "querying surface capabilities");

    VkExtent2D extent = ctx.surfaceInfo.currentExtent;

#if defined(__linux__)
    // Some Linux compositors report (0,0) or stale extents; trust GLFW instead.
    {
        int w = 0, h = 0;
        glfwGetFramebufferSize(ctx.window, &w, &h);
        extent.width                  = static_cast<uint32_t>(w);
        extent.height                 = static_cast<uint32_t>(h);
        ctx.surfaceInfo.currentExtent = extent;
    }
#endif

    // Request one more than the driver's minimum so the CPU isn't stalled
    // when the presentation engine is holding an image. Clamp to the driver's
    // max if one is advertised (maxImageCount == 0 means no upper bound).
    uint32_t desiredImageCount = ctx.surfaceInfo.minImageCount + 1u;
    if (ctx.surfaceInfo.maxImageCount > 0u &&
        desiredImageCount > ctx.surfaceInfo.maxImageCount)
    {
        desiredImageCount = ctx.surfaceInfo.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapChainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    {
        swapChainInfo.presentMode         = VK_PRESENT_MODE_FIFO_KHR;
        swapChainInfo.surface             = ctx.surface;
        swapChainInfo.minImageCount       = desiredImageCount;
        swapChainInfo.imageExtent         = extent;
        swapChainInfo.preTransform        = ctx.surfaceInfo.currentTransform;
        swapChainInfo.pQueueFamilyIndices = &ctx.selectedQueueFamilyIndex;
        swapChainInfo.imageColorSpace     = ctx.selectedSurfaceFormat.colorSpace;
        swapChainInfo.imageFormat         = ctx.selectedSurfaceFormat.format;
        swapChainInfo.imageArrayLayers    = 1u;
        swapChainInfo.imageUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }

    ThrowOnFail(vkCreateSwapchainKHR(ctx.device, &swapChainInfo, nullptr, &ctx.swapchain),
                "creating swapchain");

    ThrowOnFail(
        vkGetSwapchainImagesKHR(ctx.device, ctx.swapchain, &ctx.swapchainImageCount, nullptr),
        "querying swapchain image count");

    ctx.swapchainImages.resize(ctx.swapchainImageCount);
    ctx.swapchainImageViews.resize(ctx.swapchainImageCount);
    ctx.swapchainSemaphoreRenderComplete.resize(ctx.swapchainImageCount);

    ThrowOnFail(vkGetSwapchainImagesKHR(ctx.device,
                                        ctx.swapchain,
                                        &ctx.swapchainImageCount,
                                        ctx.swapchainImages.data()),
                "retrieving swapchain images");

    for (uint32_t imageIndex = 0u; imageIndex < ctx.swapchainImageCount; imageIndex++)
    {
        VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        ThrowOnFail(vkCreateSemaphore(ctx.device,
                                      &semaphoreInfo,
                                      nullptr,
                                      &ctx.swapchainSemaphoreRenderComplete[imageIndex]),
                    "creating swapchain render-complete semaphore");

        VkImageViewCreateInfo imageViewInfo         = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        imageViewInfo.viewType                      = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.image                         = ctx.swapchainImages[imageIndex];
        imageViewInfo.format                        = ctx.selectedSurfaceFormat.format;
        imageViewInfo.subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel = 0u;
        imageViewInfo.subresourceRange.levelCount   = 1u;
        imageViewInfo.subresourceRange.baseArrayLayer = 0u;
        imageViewInfo.subresourceRange.layerCount     = 1u;
        imageViewInfo.components                      = { VK_COMPONENT_SWIZZLE_IDENTITY,
                                                          VK_COMPONENT_SWIZZLE_IDENTITY,
                                                          VK_COMPONENT_SWIZZLE_IDENTITY,
                                                          VK_COMPONENT_SWIZZLE_IDENTITY };
        ThrowOnFail(vkCreateImageView(ctx.device,
                                      &imageViewInfo,
                                      nullptr,
                                      &ctx.swapchainImageViews[imageIndex]),
                    "creating swapchain image view");
    }
}

// Caller must ensure no GPU work is using these resources (vkDeviceWaitIdle).
static void DestroySwapchainAndPerImageResources(Context& ctx)
{
    for (auto& view : ctx.swapchainImageViews)
        vkDestroyImageView(ctx.device, view, nullptr);

    for (auto& sem : ctx.swapchainSemaphoreRenderComplete)
        vkDestroySemaphore(ctx.device, sem, nullptr);

    if (ctx.swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(ctx.device, ctx.swapchain, nullptr);

    ctx.swapchainImages.clear();
    ctx.swapchainImageViews.clear();
    ctx.swapchainSemaphoreRenderComplete.clear();
    ctx.swapchain           = VK_NULL_HANDLE;
    ctx.swapchainImageCount = 0u;
}

// Tear down the old swapchain and rebuild at the current window size. Also
// parks on glfwWaitEvents while the window is minimized (extent 0x0) so we
// don't create a zero-sized swapchain.
static void RecreateSwapchain(Context& ctx)
{
    // Park while minimized.
    int w = 0, h = 0;

    glfwGetFramebufferSize(ctx.window, &w, &h);

    while (w == 0 || h == 0)
    {
        glfwWaitEvents();

        glfwGetFramebufferSize(ctx.window, &w, &h);

        if (glfwWindowShouldClose(ctx.window))
            return;
    }

    vkDeviceWaitIdle(ctx.device);

    DestroySwapchainAndPerImageResources(ctx);
    CreateSwapchainAndPerImageResources(ctx);

    // ImGui's Vulkan backend ring size is tied to framesInFlight (see
    // CreateContext for the rationale), which does not change on resize, so
    // ImGui_ImplVulkan_SetMinImageCount is intentionally not called here.
}

// Best-effort teardown of a partially-constructed Context. Used by
// CreateContext's catch path: any handle still at VK_NULL_HANDLE / empty was
// never created and is skipped, so this is safe to call at any point.
static void DestroyPartialContext(Context& ctx, bool imguiContextCreated)
{
    if (ctx.device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(ctx.device);

    if (ctx.device != VK_NULL_HANDLE)
        DestroySwapchainAndPerImageResources(ctx);

    // Free command pools (and the command buffers they own) before draining
    // user deletion queues — see the matching note in DestroyContext for why.
    for (auto& pool : ctx.frameCommandPool)
        if (pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(ctx.device, pool, nullptr);
    for (auto& sem : ctx.frameSemaphoreImageAvailable)
        if (sem != VK_NULL_HANDLE)
            vkDestroySemaphore(ctx.device, sem, nullptr);
    for (auto& fence : ctx.frameFenceRenderComplete)
        if (fence != VK_NULL_HANDLE)
            vkDestroyFence(ctx.device, fence, nullptr);

    if (imguiContextCreated)
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    for (auto& frameDeletionQueue : ctx.frameDeletionQueues)
    {
        while (!frameDeletionQueue.empty())
        {
            frameDeletionQueue.front()();
            frameDeletionQueue.pop_front();
        }
    }

    if (ctx.surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
    if (ctx.allocator != VK_NULL_HANDLE)
        vmaDestroyAllocator(ctx.allocator);
    if (ctx.device != VK_NULL_HANDLE)
        vkDestroyDevice(ctx.device, nullptr);
    if (ctx.instance != VK_NULL_HANDLE)
        vkDestroyInstance(ctx.instance, nullptr);

    if (ctx.window != nullptr)
        glfwDestroyWindow(ctx.window);
    glfwTerminate();
}

// Implementation
// -----------------------

Context Aule::CreateContext(const Params& params)
{
    Context ctx = {};
    bool    imguiContextCreated = false;

    assert(params.windowName != nullptr);
    assert(params.windowWidth != 0);
    assert(params.windowHeight != 0);

    try
    {

    // ----------------------------------

#ifdef __linux__
    // X11 is better than Wayland in this case due to better RADV tracing support.
    // And also a weird bug in imgui scaling that I am too lazy to fix at the moment.
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

    ThrowOnFail(glfwInit() == GLFW_TRUE, "glfwInit failed");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    ctx.window = glfwCreateWindow(params.windowWidth,
                                  params.windowHeight,
                                  params.windowName,
                                  nullptr,
                                  nullptr);

    ThrowOnFail(ctx.window != nullptr, "glfwCreateWindow returned null");

    ThrowOnFail(volkInitialize(), "initializing volk (no Vulkan loader found?)");

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

    ThrowOnFail(vkCreateInstance(&instanceInfo, nullptr, &ctx.instance),
                "creating Vulkan instance");

    volkLoadInstance(ctx.instance);

    // ----------------------------------

    uint32_t physicalDeviceCount;
    ThrowOnFail(vkEnumeratePhysicalDevices(ctx.instance, &physicalDeviceCount, nullptr),
                "enumerating physical device count");

    // No drivers found!
    ThrowOnFail(physicalDeviceCount > 0,
                "no Vulkan-capable physical devices found (is a GPU driver installed?)");

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    ThrowOnFail(
        vkEnumeratePhysicalDevices(ctx.instance, &physicalDeviceCount, physicalDevices.data()),
        "enumerating physical devices");

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

        // Grab the first graphics-capable queue family and stop.
        ctx.selectedQueueFamilyIndex = queueFamilyIndex;
        break;
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
            ThrowOnFail(
                DeviceExtensionSupported(requestedExtension),
                (std::string("required device extension not supported: ") + requestedExtension)
                    .c_str());
    }

    VkPhysicalDeviceFeatures2                features                = {};
    VkPhysicalDeviceSynchronization2Features featureSync2            = {};
    VkPhysicalDeviceDynamicRenderingFeatures featureDynamicRendering = {};
    VkPhysicalDeviceScalarBlockLayoutFeatures featureScalarLayout    = {};

    features.sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    featureSync2.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    featureDynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    featureScalarLayout.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;

    features.pNext                = &featureSync2;
    featureSync2.pNext            = &featureDynamicRendering;
    featureDynamicRendering.pNext = &featureScalarLayout;

    featureSync2.synchronization2            = VK_TRUE;
    featureDynamicRendering.dynamicRendering = VK_TRUE;
    featureScalarLayout.scalarBlockLayout    = VK_TRUE;

    deviceInfo.pNext                   = &features;
    deviceInfo.queueCreateInfoCount    = queueCreateInfos.size();
    deviceInfo.pQueueCreateInfos       = queueCreateInfos.data();
    deviceInfo.enabledExtensionCount   = extensions.size();
    deviceInfo.ppEnabledExtensionNames = extensions.data();

    ThrowOnFail(vkCreateDevice(ctx.selectedPhysicalDevice, &deviceInfo, nullptr, &ctx.device),
                "creating Vulkan logical device");

    volkLoadDevice(ctx.device);

    for (uint32_t queueFamilyIndex = 0u; queueFamilyIndex < ctx.queueFamilyCount;
         queueFamilyIndex++)
    {
        // Load queues into the map
        vkGetDeviceQueue(ctx.device, queueFamilyIndex, 0u, &ctx.queues[queueFamilyIndex]);
    }

    // Surface
    // ---------------------

    ThrowOnFail(glfwCreateWindowSurface(ctx.instance, ctx.window, nullptr, &ctx.surface),
                "creating window surface (glfwCreateWindowSurface)");

    uint32_t surfaceFormatCount;
    ThrowOnFail(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.selectedPhysicalDevice,
                                                     ctx.surface,
                                                     &surfaceFormatCount,
                                                     nullptr),
                "querying surface format count");

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    ThrowOnFail(vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.selectedPhysicalDevice,
                                                     ctx.surface,
                                                     &surfaceFormatCount,
                                                     surfaceFormats.data()),
                "retrieving surface formats");

    // Prefer a well-behaved 8-bit BGRA format (sRGB or UNORM) so ImGui colors
    // render correctly. Some drivers advertise an HDR/wide-gamut format at
    // index 0, which makes a naive selection look wrong. Fall back to index 0
    // if nothing matches.
    ctx.selectedSurfaceFormat = surfaceFormats.at(0);
    for (const auto& candidate : surfaceFormats)
    {
        const bool isPreferredFormat = candidate.format == VK_FORMAT_B8G8R8A8_SRGB ||
                                       candidate.format == VK_FORMAT_B8G8R8A8_UNORM ||
                                       candidate.format == VK_FORMAT_R8G8B8A8_SRGB ||
                                       candidate.format == VK_FORMAT_R8G8B8A8_UNORM;
        if (isPreferredFormat && candidate.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
        {
            ctx.selectedSurfaceFormat = candidate;
            break;
        }
    }

    CreateSwapchainAndPerImageResources(ctx);

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
                                      &ctx.frameSemaphoreImageAvailable[frameInFlightIndex]),
                    "creating frame image-available semaphore");

        VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;
        ThrowOnFail(vkCreateFence(ctx.device,
                                  &fenceInfo,
                                  nullptr,
                                  &ctx.frameFenceRenderComplete[frameInFlightIndex]),
                    "creating frame render-complete fence");

        VkCommandPoolCreateInfo commandPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        commandPoolInfo.queueFamilyIndex        = ctx.selectedQueueFamilyIndex;
        ThrowOnFail(vkCreateCommandPool(ctx.device,
                                        &commandPoolInfo,
                                        nullptr,
                                        &ctx.frameCommandPool[frameInFlightIndex]),
                    "creating frame command pool");

        VkCommandBufferAllocateInfo commandAllocateInfo = {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
        };
        commandAllocateInfo.commandBufferCount = 1u;
        commandAllocateInfo.commandPool        = ctx.frameCommandPool[frameInFlightIndex];
        ThrowOnFail(vkAllocateCommandBuffers(ctx.device,
                                             &commandAllocateInfo,
                                             &ctx.frameCommandBuffer[frameInFlightIndex]),
                    "allocating frame command buffer");
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
    ThrowOnFail(vmaCreateAllocator(&allocatorInfo, &ctx.allocator), "creating VMA allocator");

    // -----------------------

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    imguiContextCreated = true;

    ImGui_ImplGlfw_InitForVulkan(ctx.window, true);

    ImGui_ImplVulkan_InitInfo imguiInfo = {};
    {
        imguiInfo.Instance            = ctx.instance;
        imguiInfo.PhysicalDevice      = ctx.selectedPhysicalDevice;
        imguiInfo.Device              = ctx.device;
        imguiInfo.QueueFamily         = ctx.selectedQueueFamilyIndex;
        imguiInfo.Queue               = ctx.queues[ctx.selectedQueueFamilyIndex];
        // ImGui's Vulkan backend keeps an internal ring of per-frame vertex/
        // index buffers sized by ImageCount and advances one slot per
        // RenderDrawData call. When the user's UI grows (e.g. opens a panel),
        // the backend reallocates the slot's buffer SYNCHRONOUSLY, destroying
        // the old one. For that destroy to be safe, the slot's prior
        // submission must have completed. Sizing the ring to framesInFlight
        // (not swapchainImageCount) means a full ring cycle == framesInFlight
        // RenderDrawData calls, by which point our per-frame fence wait has
        // guaranteed the GPU is done with that slot.
        imguiInfo.MinImageCount       = ctx.framesInFlight;
        imguiInfo.ImageCount          = ctx.framesInFlight;
        imguiInfo.UseDynamicRendering = true;
        imguiInfo.DescriptorPoolSize  = params.maxSupportedImguiImages;

        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1u;
        imguiInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats =
            &ctx.selectedSurfaceFormat.format;
    }

    ImGui_ImplVulkan_Init(&imguiInfo);

    // -----------------------

    return ctx;

    }
    catch (...)
    {
        // Any partially-constructed Vulkan/GLFW/ImGui state gets torn down
        // before the exception propagates to the caller.
        DestroyPartialContext(ctx, imguiContextCreated);
        throw;
    }
}

void Aule::DestroyContext(Context& context)
{
    vkDeviceWaitIdle(context.device);

    // Per-swapchain-image resources (incl. swapchain itself).
    DestroySwapchainAndPerImageResources(context);

    // Per-frame-in-flight resources. Destroy the command pools FIRST: this
    // frees every command buffer they own and releases the validation-layer
    // "in use" reference count on any buffers/images those command buffers
    // recorded against. Without this, draining the user deletion queues
    // below trips VUID-vkDestroyBuffer-buffer-00922 even though the GPU has
    // long been idle (the spec is satisfied; the validation tracker is more
    // conservative).
    for (uint32_t i = 0u; i < context.framesInFlight; i++)
    {
        vkDestroyCommandPool(context.device, context.frameCommandPool[i], nullptr);
        vkDestroySemaphore(context.device, context.frameSemaphoreImageAvailable[i], nullptr);
        vkDestroyFence(context.device, context.frameFenceRenderComplete[i], nullptr);
    }

    // ImGui's Vulkan backend owns its own per-frame vertex/index buffers.
    // Shut it down before draining user deletion queues so its command-buffer
    // references are released too, and before vmaDestroyAllocator / device
    // teardown.
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Now safe to run any pending per-frame deletion lambdas (typically
    // user-owned vkDestroyBuffer / vmaDestroyImage calls).
    for (auto& frameDeletionQueue : context.frameDeletionQueues)
    {
        while (!frameDeletionQueue.empty())
        {
            frameDeletionQueue.front()();
            frameDeletionQueue.pop_front();
        }
    }

    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    vmaDestroyAllocator(context.allocator);
    vkDestroyDevice(context.device, nullptr);
    vkDestroyInstance(context.instance, nullptr);

    glfwDestroyWindow(context.window);
    glfwTerminate();
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

        uint32_t       swapchainIndex;
        const VkResult acquireResult =
            vkAcquireNextImage2KHR(ctx.device, &swapChainIndexAcquireInfo, &swapchainIndex);

        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // Swapchain no longer matches surface (resize, etc.). Rebuild and
            // skip this frame. The fence was already reset; re-signal it so
            // the next iteration's wait completes immediately.
            RecreateSwapchain(ctx);

            vkQueueSubmit(ctx.queues[ctx.selectedQueueFamilyIndex],
                          0u,
                          nullptr,
                          ctx.frameFenceRenderComplete[frameInFlightIndex]);

            continue;
        }
        // VK_SUBOPTIMAL_KHR from acquire is still a successful acquire; render
        // this frame and handle the rebuild after present.
        ThrowOnFail(acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR,
                    "vkAcquireNextImage2KHR returned an unexpected result");

        auto& cmd = ctx.frameCommandBuffer[frameInFlightIndex];

        ThrowOnFail(vkResetCommandPool(ctx.device, ctx.frameCommandPool[frameInFlightIndex], 0x0),
                    "resetting frame command pool");

        VkCommandBufferBeginInfo cmdInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        ThrowOnFail(vkBeginCommandBuffer(cmd, &cmdInfo), "beginning frame command buffer");

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

        // If the user provided a mutex, hold it across ImGui's render + draw
        // submission (ImGui's Vulkan backend can issue internal work). The
        // lock must outlive both calls below — a `std::lock_guard` declared
        // inside an `if` body would die at the next `;` and protect nothing.
        std::unique_lock<std::mutex> dispatchLock;
        if (pDispatchQueueMutex)
            dispatchLock = std::unique_lock<std::mutex>(*pDispatchQueueMutex);

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

        ThrowOnFail(vkEndCommandBuffer(cmd), "ending frame command buffer");

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
                                  ctx.frameFenceRenderComplete[frameInFlightIndex]),
                    "submitting frame command buffer");

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        {
            presentInfo.swapchainCount     = 1u;
            presentInfo.pSwapchains        = &ctx.swapchain;
            presentInfo.pImageIndices      = &swapchainIndex;
            presentInfo.waitSemaphoreCount = 1u;
            presentInfo.pWaitSemaphores    = &ctx.swapchainSemaphoreRenderComplete[swapchainIndex];
        }
        const VkResult presentResult =
            vkQueuePresentKHR(ctx.queues[ctx.selectedQueueFamilyIndex], &presentInfo);

        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
            RecreateSwapchain(ctx);
        else
            ThrowOnFail(presentResult, "presenting swapchain image");

        // -----------------------

        frameInFlightIndex = (frameInFlightIndex + 1u) % ctx.framesInFlight;
    }
}
