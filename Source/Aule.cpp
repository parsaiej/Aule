#include "../Include/Aule/Aule.h"

inline void ThrowOnFail(VkResult result)
{
    if (result != VK_SUCCESS)
        throw std::runtime_error("Internal Vulkan call failed.");
}

// State
// -----------------------

GLFWwindow* gp_Window;

VkInstance               g_VkInstance;
VkPhysicalDevice         g_VkPhysicalDevice;
VkDevice                 g_VkLogicalDevice;
uint32_t                 g_VkGraphicsQueueIndex;
VkQueue                  g_VkGraphicsQueue;
VkSurfaceKHR             g_VkSurface;
VkSurfaceCapabilitiesKHR g_VkSurfaceInfo;
VkSemaphore              g_VkImageAvailableSemaphore;
VkSemaphore              g_VkRenderCompleteSemaphore;
VkSwapchainKHR           g_VkSwapchain;
uint32_t                 g_VkSwapchainImageCount;
std::vector<VkImage>     g_VkSwapchainImages;
std::vector<VkImageView> g_VkSwapchainImageViews;
VkFormat                 g_VkSwapchainImageFormat;

// Implementation
// -----------------------

void Aule::Go(const Params& params)
{
    // Create operating system window.
    // ----------------------------------

    if (!glfwInit())
        exit(1);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    gp_Window = glfwCreateWindow(params.windowWidth, params.windowHeight, params.windowName, nullptr, nullptr);

    if (!gp_Window)
        exit(1);

    ThrowOnFail(volkInitialize());

    // Instance
    // ----------------------------------

    VkApplicationInfo applicationInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    {
        applicationInfo.pApplicationName   = params.windowName;
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName        = "No Engine";
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

    ThrowOnFail(vkCreateInstance(&instanceInfo, nullptr, &g_VkInstance));

    // Resolve function pointers for instance.
    volkLoadInstance(g_VkInstance);

    // Physical Device
    // ----------------------------------

    uint32_t physicalDeviceCount;
    ThrowOnFail(vkEnumeratePhysicalDevices(g_VkInstance, &physicalDeviceCount, nullptr));

    if (physicalDeviceCount == 0)
        exit(1);

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    ThrowOnFail(vkEnumeratePhysicalDevices(g_VkInstance, &physicalDeviceCount, physicalDevices.data()));

    // Select device #0 for now.
    g_VkPhysicalDevice = physicalDevices[0];

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(g_VkPhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueInfos(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(g_VkPhysicalDevice, &queueFamilyCount, queueInfos.data());

    for (uint32_t queueInfoIndex = 0u; queueInfoIndex < queueFamilyCount; queueInfoIndex++)
    {
        if (!(queueInfos[queueInfoIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            continue;

        // Just grab first graphics compatible queue.
        g_VkGraphicsQueueIndex = queueInfoIndex;

        break;
    }

    // Logical Device
    // ----------------------------------

    VkDeviceQueueCreateInfo queueCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };

    const float kGraphicsQueuePriority = 1.0f;

    queueCreateInfo.queueFamilyIndex = g_VkGraphicsQueueIndex;
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

    ThrowOnFail(vkCreateDevice(g_VkPhysicalDevice, &deviceInfo, nullptr, &g_VkLogicalDevice));

    // Resolve function pointers for device.
    volkLoadDevice(g_VkLogicalDevice);

    // Fetch queue
    vkGetDeviceQueue(g_VkLogicalDevice, g_VkGraphicsQueueIndex, 0u, &g_VkGraphicsQueue);

    // Surface
    // ---------------------

    ThrowOnFail(glfwCreateWindowSurface(g_VkInstance, gp_Window, nullptr, &g_VkSurface));

    // Swapchain
    // ---------------------

    uint32_t surfaceFormatCount;
    ThrowOnFail(vkGetPhysicalDeviceSurfaceFormatsKHR(g_VkPhysicalDevice, g_VkSurface, &surfaceFormatCount, nullptr));

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    ThrowOnFail(vkGetPhysicalDeviceSurfaceFormatsKHR(g_VkPhysicalDevice, g_VkSurface, &surfaceFormatCount, surfaceFormats.data()));
    ThrowOnFail(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_VkPhysicalDevice, g_VkSurface, &g_VkSurfaceInfo));

    VkSwapchainCreateInfoKHR swapChainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    {
        swapChainInfo.presentMode         = VK_PRESENT_MODE_FIFO_KHR;
        swapChainInfo.surface             = g_VkSurface;
        swapChainInfo.minImageCount       = g_VkSurfaceInfo.minImageCount;
        swapChainInfo.imageExtent         = g_VkSurfaceInfo.currentExtent;
        swapChainInfo.preTransform        = g_VkSurfaceInfo.currentTransform;
        swapChainInfo.pQueueFamilyIndices = &g_VkGraphicsQueueIndex;
        swapChainInfo.imageColorSpace     = surfaceFormats.at(0).colorSpace;
        swapChainInfo.imageFormat         = surfaceFormats.at(0).format;
        swapChainInfo.imageArrayLayers    = 1u;
        swapChainInfo.imageUsage          = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapChainInfo.compositeAlpha      = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    ThrowOnFail(vkCreateSwapchainKHR(g_VkLogicalDevice, &swapChainInfo, nullptr, &g_VkSwapchain));

    g_VkSwapchainImageFormat = swapChainInfo.imageFormat;

    ThrowOnFail(vkGetSwapchainImagesKHR(g_VkLogicalDevice, g_VkSwapchain, &g_VkSwapchainImageCount, nullptr));

    g_VkSwapchainImages.resize(g_VkSwapchainImageCount);
    g_VkSwapchainImageViews.resize(g_VkSwapchainImageCount);

    // Swapchain Image Views
    // ---------------------

    ThrowOnFail(vkGetSwapchainImagesKHR(g_VkLogicalDevice, g_VkSwapchain, &g_VkSwapchainImageCount, g_VkSwapchainImages.data()));

    for (uint32_t swapChainImageIndex = 0u; swapChainImageIndex < g_VkSwapchainImageCount; swapChainImageIndex++)
    {
        VkImageViewCreateInfo imageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };

        imageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.image                           = g_VkSwapchainImages[swapChainImageIndex];
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

        ThrowOnFail(vkCreateImageView(g_VkLogicalDevice, &imageViewInfo, nullptr, &g_VkSwapchainImageViews[swapChainImageIndex]));
    }
}
