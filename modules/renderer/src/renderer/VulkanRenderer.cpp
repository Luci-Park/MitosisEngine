/**
 * @file VulkanRenderer.cpp
 * @author Sumin Park
 * @brief Renderer with all Vulkan signatures.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include "renderer/VulkanRenderer.h"
#include "vulkan/VulkanSurface.h"
#include <core/log/Log.h>
#include <core/log/Assert.h>

#include <vk_mem_alloc.h>

#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace mts
{
    namespace
    {
        VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT *data,
            void *)
        {
            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            {
                MTS_LOG_ERROR("[vulkan] {}", data->pMessage);
                MTS_DEBUG_BREAK();
            }
            else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
            {
                MTS_LOG_WARN("[vulkan] {}", data->pMessage);
            }
            else
            {
                MTS_LOG_INFO("[vulkan] {}", data->pMessage);
            }

            // VK_TRUE would abort the call that triggered this. That is a layer
            // self-test mechanism, not an error handler. Always VK_FALSE.
            return VK_FALSE;
        }

        VkDebugUtilsMessengerCreateInfoEXT MakeMessengerCreateInfo()
        {
            return VkDebugUtilsMessengerCreateInfoEXT{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = &DebugCallback};
        }

        bool HasLayer(const std::vector<VkLayerProperties> &layers, const char *name)
        {
            for (const VkLayerProperties &layer : layers)
            {
                if (std::strcmp(layer.layerName, name) == 0)
                    return true;
            }
            return false;
        }

        bool HasExtension(const std::vector<VkExtensionProperties> &exts, const char *name)
        {
            for (const VkExtensionProperties &ext : exts)
            {
                if (std::strcmp(ext.extensionName, name) == 0)
                    return true;
            }
            return false;
        }

        bool HasDeviceExtension(VkPhysicalDevice device, const char *name)
        {
            uint32_t count = 0;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
            std::vector<VkExtensionProperties> exts(count);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &count, exts.data());

            for (const VkExtensionProperties &ext : exts)
            {
                if (std::strcmp(ext.extensionName, name) == 0)
                    return true;
            }
            return false;
        }

        bool HasRequiredFeatures(VkPhysicalDevice device)
        {
            VkPhysicalDeviceVulkan13Features features13{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

            VkPhysicalDeviceVulkan12Features features12{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                .pNext = &features13};

            VkPhysicalDeviceFeatures2 features{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &features12};

            vkGetPhysicalDeviceFeatures2(device, &features);

            return features13.dynamicRendering == VK_TRUE &&
                   features13.synchronization2 == VK_TRUE &&
                   features12.timelineSemaphore == VK_TRUE;
        }
        bool HasSurfaceSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            uint32_t formatCount = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

            uint32_t presentModeCount = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

            return formatCount > 0 && presentModeCount > 0;
        }

        uint32_t FindGraphicsPresentFamily(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            uint32_t count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

            for (uint32_t i = 0; i < count; ++i)
            {
                if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
                    continue;

                VkBool32 presentSupported = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupported);

                if (presentSupported == VK_TRUE)
                    return i;
            }
            return UINT32_MAX;
        }

        VkSurfaceFormatKHR ChooseSurfaceFormat(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            uint32_t count = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
            std::vector<VkSurfaceFormatKHR> formats(count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, formats.data());

            for (const VkSurfaceFormatKHR &format : formats)
            {
                if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                    return format;
            }

            // fall back to basic format. It'll have the basics
            return formats[0];
        }

        VkPresentModeKHR ChoosePresentMode(VkPhysicalDevice device, VkSurfaceKHR surface)
        {
            uint32_t count = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
            std::vector<VkPresentModeKHR> modes(count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, modes.data());

            for (VkPresentModeKHR mode : modes)
            {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                    return mode;
            }

            // FIFO is the only mode the spec guarantees exists.
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR &caps, const Window &window)
        {
            if (caps.currentExtent.width != UINT32_MAX)
                return caps.currentExtent;

            // Framebuffer size, not window size: they differ on HiDPI.
            VkExtent2D extent{window.Width(), window.Height()};

            extent.width = std::clamp(extent.width,
                                      caps.minImageExtent.width,
                                      caps.maxImageExtent.width);
            extent.height = std::clamp(extent.height,
                                       caps.minImageExtent.height,
                                       caps.maxImageExtent.height);
            return extent;
        }
        void ImageBarrier(VkCommandBuffer cmd, VkImage image,
                          VkImageLayout oldLayout, VkImageLayout newLayout,
                          VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                          VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess)
        {
            const VkImageMemoryBarrier2 barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = srcStage,
                .srcAccessMask = srcAccess,
                .dstStageMask = dstStage,
                .dstAccessMask = dstAccess,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .levelCount = 1,
                    .layerCount = 1}};

            const VkDependencyInfo dep{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &barrier};

            vkCmdPipelineBarrier2(cmd, &dep);
        }
    }

    bool VulkanRenderer::Initialize(const RendererDesc &desc)
    {
        if (desc.window == nullptr)
        {
            MTS_LOG_ERROR("no window");
            return false;
        }

        m_Window = desc.window;

        if (volkInitialize() != VK_SUCCESS)
        {
            MTS_LOG_ERROR("volk initialized failed");
            return false;
        }

        if (!CreateVulkanInstance(desc))
            return false;

        volkLoadInstance(m_VulkanInstance);

        if (m_ValidationEnabled && !CreateDebugMessenger())
            return false;

        if (!CreateSurface())
            return false;

        if (!FindPhysicalDevice())
            return false;

        if (!CreateLogicalDevice())
            return false;

        if (!CreateAllocator())
            return false;

        if (!CreateSwapchain())
            return false;

        if (!CreateFrameResources())
            return false;

        return true;
    }
    void VulkanRenderer::Shutdown()
    {
        if (m_Device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(m_Device);

            if (m_Timeline != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_Device, m_Timeline, nullptr);
                m_Timeline = VK_NULL_HANDLE;
            }

            for (uint32_t i = 0; i < kFramesInFlight; ++i)
            {
                if (m_ImageAcquired[i] != VK_NULL_HANDLE)
                    vkDestroySemaphore(m_Device, m_ImageAcquired[i], nullptr);
                // Destroying the pool frees its command buffers too.
                if (m_CmdPools[i] != VK_NULL_HANDLE)
                    vkDestroyCommandPool(m_Device, m_CmdPools[i], nullptr);
            }

            DestroyRenderCompleteSemaphores();
            DestroySwapchain();
        }

        if (m_Allocator != VK_NULL_HANDLE)
        {
            // The permanent leak detector. Every later milestone that allocates
            // must still land here at 0 bytes.
            VmaTotalStatistics stats{};
            vmaCalculateStatistics(m_Allocator, &stats);
            MTS_LOG_INFO("[vma] live bytes at shutdown: {} in {} allocation(s)",
                         stats.total.statistics.allocationBytes,
                         stats.total.statistics.allocationCount);

            vmaDestroyAllocator(m_Allocator);
            m_Allocator = VK_NULL_HANDLE;
        }

        if (m_Device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_Device, nullptr);
            m_Device = VK_NULL_HANDLE;
        }

        if (m_DebugMessenger != VK_NULL_HANDLE)
        {
            vkDestroyDebugUtilsMessengerEXT(m_VulkanInstance, m_DebugMessenger, nullptr);
            m_DebugMessenger = VK_NULL_HANDLE;
        }

        if (m_Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_VulkanInstance, m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }

        if (m_VulkanInstance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_VulkanInstance, nullptr);
            m_VulkanInstance = VK_NULL_HANDLE;
        }

        volkFinalize();
    }
    bool VulkanRenderer::CreateVulkanInstance(const RendererDesc &desc)
    {
        // to fill data, read twice : once for count, once to fill
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        uint32_t extCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availableExts(extCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availableExts.data());

        std::vector<const char *> layers;
        std::vector<const char *> extensions;

        const WindowBackend backend = m_Window->NativeWindow().backend;

        const char *platformExt = vk::PlatformSurfaceExtension(backend);
        if (platformExt == nullptr)
        {
            MTS_LOG_CRITICAL("No Vulkan surface extension for window backend {}",
                             static_cast<int>(backend));
            return false;
        }

        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        extensions.push_back(platformExt);

        for (const char *name : extensions)
        {
            if (!HasExtension(availableExts, name))
            {
                MTS_LOG_CRITICAL("Required instance extension missing: {}", name);
                return false;
            }
        }

        // debug soft check
        if (desc.enableValidation)
        {
            const bool hasLayer = HasLayer(availableLayers, "VK_LAYER_KHRONOS_validation");
            const bool hasDebugUtils = HasExtension(availableExts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

            if (hasLayer && hasDebugUtils)
            {
                layers.push_back("VK_LAYER_KHRONOS_validation");
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                m_ValidationEnabled = true;
            }
            else
            {
                MTS_LOG_WARN("Validation unavailable (layer: {}, debug_utils: {}); "
                             "install the Vulkan SDK to enable it",
                             hasLayer, hasDebugUtils);
            }
        }

        VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = desc.appName,
            .apiVersion = VulkanVersion};

        // temporary for instance creation.
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo = MakeMessengerCreateInfo();

        VkInstanceCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = m_ValidationEnabled ? &messengerInfo : nullptr,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(layers.size()),
            .ppEnabledLayerNames = layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data()};

        if (vkCreateInstance(&createInfo, nullptr, &m_VulkanInstance) != VK_SUCCESS)
        {
            MTS_LOG_CRITICAL("vkCreateInstance failed");
            return false;
        }

        return true;
    }
    bool VulkanRenderer::CreateDebugMessenger()
    {
        const VkDebugUtilsMessengerCreateInfoEXT info = MakeMessengerCreateInfo();

        if (vkCreateDebugUtilsMessengerEXT(m_VulkanInstance, &info, nullptr, &m_DebugMessenger) != VK_SUCCESS)
        {
            MTS_LOG_ERROR("vkCreateDebugUtilsMessengerEXT failed");
            return false;
        }
        return true;
    }
    bool VulkanRenderer::CreateSurface()
    {
        m_Surface = vk::CreateSurface(m_VulkanInstance, m_Window->NativeWindow());

        if (m_Surface == VK_NULL_HANDLE)
        {
            MTS_LOG_CRITICAL("Vulkan surface creation failed");
            return false;
        }
        return true;
    }

    bool VulkanRenderer::FindPhysicalDevice()
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(m_VulkanInstance, &count, nullptr);
        if (count == 0)
        {
            MTS_LOG_CRITICAL("No Vulkan-capable GPU found");
            return false;
        }

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_VulkanInstance, &count, devices.data());

        uint32_t bestScore = 0;

        for (VkPhysicalDevice device : devices)
        {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(device, &props);

            // Every rejection says which device and why
            if (props.apiVersion < VulkanVersion)
            {
                MTS_LOG_INFO("Rejected {}: Vulkan {}.{}, need 1.3", props.deviceName,
                             VK_API_VERSION_MAJOR(props.apiVersion),
                             VK_API_VERSION_MINOR(props.apiVersion));
                continue;
            }
            if (!HasDeviceExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            {
                MTS_LOG_INFO("Rejected {}: no swapchain extension", props.deviceName);
                continue;
            }
            if (!HasRequiredFeatures(device))
            {
                MTS_LOG_INFO("Rejected {}: missing dynamicRendering / synchore", props.deviceName);
                continue;
            }
            if (!HasSurfaceSupport(device, m_Surface))
            {
                MTS_LOG_INFO("Rejected {}: no surface formats or present modes", props.deviceName);
                continue;
            }

            const uint32_t family = FindGraphicsPresentFamily(device, m_Surface);
            if (family == UINT32_MAX)
            {
                MTS_LOG_INFO("Rejected {}: no graphics+present queue family", props.deviceName);
                continue;
            }

            const uint32_t score =
                (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 1000 : 100;

            if (score > bestScore)
            {
                bestScore = score;
                m_PhysicalDevice = device;
                m_GfxQueueFamIdx = family;
            }
        }

        if (m_PhysicalDevice == VK_NULL_HANDLE)
        {
            MTS_LOG_CRITICAL("No suitable GPU among {} candidate(s)", count);
            return false;
        }

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
        MTS_LOG_INFO("GPU: {} | {} | Vulkan {}.{}.{} | graphics+present family {}",
                     props.deviceName,
                     props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "discrete" : "integrated",
                     VK_API_VERSION_MAJOR(props.apiVersion),
                     VK_API_VERSION_MINOR(props.apiVersion),
                     VK_API_VERSION_PATCH(props.apiVersion),
                     m_GfxQueueFamIdx);
        return true;
    }

    bool VulkanRenderer::CreateLogicalDevice()
    {
        // required even for single queue, needed in infostruct
        const float queuePriority = 1.0f;

        const VkDeviceQueueCreateInfo queueInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = m_GfxQueueFamIdx,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority};

        // list of extensions at 1.3
        // dynamic rendering, sync resources are core, not extensions
        // .pNext to link multiple extensions together
        const char *deviceExtensions[]{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkPhysicalDeviceVulkan13Features features13{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = VK_TRUE,
            .dynamicRendering = VK_TRUE};

        VkPhysicalDeviceVulkan12Features features12{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &features13,
            .descriptorIndexing = VK_TRUE,
            .timelineSemaphore = VK_TRUE,
            .bufferDeviceAddress = VK_TRUE};

        VkPhysicalDeviceFeatures2 features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &features12};

        const VkDeviceCreateInfo deviceInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &features,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueInfo,
            .enabledExtensionCount = 1,
            .ppEnabledExtensionNames = deviceExtensions,
            .pEnabledFeatures = nullptr // must be nullptr, everything goes through .pNext
        };

        if (vkCreateDevice(m_PhysicalDevice, &deviceInfo, nullptr, &m_Device) != VK_SUCCESS)
        {
            MTS_LOG_CRITICAL("vkCreateDevice Failed");
            return false;
        }

        volkLoadDevice(m_Device);
        vkGetDeviceQueue(m_Device, m_GfxQueueFamIdx, 0, &m_GfxQueue);

        return true;
    }
    bool VulkanRenderer::CreateAllocator()
    {
        const VmaVulkanFunctions functions{
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = vkGetDeviceProcAddr};

        const VmaAllocatorCreateInfo allocatorInfo{
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = m_PhysicalDevice,
            .device = m_Device,
            .pVulkanFunctions = &functions,
            .instance = m_VulkanInstance,
            .vulkanApiVersion = VulkanVersion};

        if (vmaCreateAllocator(&allocatorInfo, &m_Allocator) != VK_SUCCESS)
        {
            MTS_LOG_CRITICAL("vmaCreateAllocator failed");
            return false;
        }
        return true;
    }
    bool VulkanRenderer::CreateSwapchain()
    {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &caps);

        const VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(m_PhysicalDevice, m_Surface);
        const VkPresentModeKHR presentMode = ChoosePresentMode(m_PhysicalDevice, m_Surface);
        const VkExtent2D extent = ChooseExtent(caps, *m_Window);

        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
            imageCount = caps.maxImageCount;

        VkSwapchainKHR oldSwapchain = m_Swapchain;

        const VkSwapchainCreateInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = m_Surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            // One family does graphics and present, so no ownership transfers.
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .preTransform = caps.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = presentMode,
            // Allow the driver to skip shading pixels hidden by other windows.
            .clipped = VK_TRUE,
            // reuse old resources
            .oldSwapchain = oldSwapchain};

        if (vkCreateSwapchainKHR(m_Device, &info, nullptr, &m_Swapchain) != VK_SUCCESS)
        {
            MTS_LOG_CRITICAL("vkCreateSwapchainKHR failed");
            m_Swapchain = VK_NULL_HANDLE;
            return false;
        }

        // Retired, not owned by the new swapchain: destroy it ourselves.
        if (oldSwapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(m_Device, oldSwapchain, nullptr);

        m_SwapchainFormat = surfaceFormat.format;
        m_SwapchainExtent = extent;

        // The swapchain creates and owns these. We never allocate or free them.
        uint32_t actualCount = 0;
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &actualCount, nullptr);
        m_SwapchainImages.resize(actualCount);
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &actualCount, m_SwapchainImages.data());

        MTS_LOG_INFO("Swapchain: {}x{} | {} image(s), requested {} | present mode {}",
                     extent.width, extent.height, actualCount, imageCount,
                     presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO");

        return CreateImageViews();
    }
    bool VulkanRenderer::CreateImageViews()
    {
        m_SwapchainViews.resize(m_SwapchainImages.size(), VK_NULL_HANDLE);

        for (size_t i = 0; i < m_SwapchainImages.size(); ++i)
        {
            const VkImageViewCreateInfo info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = m_SwapchainImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = m_SwapchainFormat,
                .subresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1}};

            if (vkCreateImageView(m_Device, &info, nullptr, &m_SwapchainViews[i]) != VK_SUCCESS)
            {
                MTS_LOG_CRITICAL("vkCreateImageView failed for swapchain image {}", i);
                return false;
            }
        }

        return true;
    }
    void VulkanRenderer::DestroySwapchain()
    {
        // The views are ours. The images are not -- never destroy those.
        for (VkImageView view : m_SwapchainViews)
        {
            if (view != VK_NULL_HANDLE)
                vkDestroyImageView(m_Device, view, nullptr);
        }
        m_SwapchainViews.clear();
        m_SwapchainImages.clear();

        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }
    }

    bool VulkanRenderer::RecreateSwapchain()
    {
        vkDeviceWaitIdle(m_Device);
        DestroyRenderCompleteSemaphores();
        DestroySwapchain();

        if (!CreateSwapchain())
            return false;
        if (!CreateRenderCompleteSemaphores())
            return false;

        m_NeedRecreate = false;
        return true;
    }

    bool VulkanRenderer::CreateFrameResources()
    {
        const VkCommandPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = 0,
            .queueFamilyIndex = m_GfxQueueFamIdx};

        const VkSemaphoreCreateInfo semInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

        for (uint32_t i = 0; i < kFramesInFlight; ++i)
        {
            if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CmdPools[i]) != VK_SUCCESS)
            {
                MTS_LOG_CRITICAL("vkCreateCommandPool failed for frame {}", i);
                return false;
            }

            const VkCommandBufferAllocateInfo allocInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = m_CmdPools[i],
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1};

            if (vkAllocateCommandBuffers(m_Device, &allocInfo, &m_CmdBuffers[i]) != VK_SUCCESS)
            {
                MTS_LOG_CRITICAL("vkAllocateCommandBuffers failed for frame {}", i);
                return false;
            }

            if (vkCreateSemaphore(m_Device, &semInfo, nullptr, &m_ImageAcquired[i]) != VK_SUCCESS)
            {
                MTS_LOG_CRITICAL("image-acquired semaphore failed for frame {}", i);
                return false;
            }
        }

        // Paired with m_NextSignalValue = kFramesInFlight + 1
        const VkSemaphoreTypeCreateInfo typeInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = kFramesInFlight};

        const VkSemaphoreCreateInfo timelineInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &typeInfo};

        if (vkCreateSemaphore(m_Device, &timelineInfo, nullptr, &m_Timeline) != VK_SUCCESS)
        {
            MTS_LOG_CRITICAL("timeline semaphore creation failed");
            return false;
        }

        return CreateRenderCompleteSemaphores();
    }

    bool VulkanRenderer::CreateRenderCompleteSemaphores()
    {
        const VkSemaphoreCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

        m_RenderComplete.resize(m_SwapchainImages.size(), VK_NULL_HANDLE);

        for (VkSemaphore &sem : m_RenderComplete)
        {
            if (vkCreateSemaphore(m_Device, &info, nullptr, &sem) != VK_SUCCESS)
            {
                MTS_LOG_CRITICAL("render-complete semaphore creation failed");
                return false;
            }
        }
        return true;
    }

    void VulkanRenderer::DestroyRenderCompleteSemaphores()
    {
        for (VkSemaphore sem : m_RenderComplete)
        {
            if (sem != VK_NULL_HANDLE)
                vkDestroySemaphore(m_Device, sem, nullptr);
        }
        m_RenderComplete.clear();
    }
    void VulkanRenderer::RecordCommands(VkCommandBuffer cmd, uint32_t imageIndex)
    {
        // get image ready
        ImageBarrier(cmd, m_SwapchainImages[imageIndex],
                     VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        // just a little test
        static uint64_t frameCounter = 0;
        const float t = static_cast<float>(frameCounter++) * 0.01f;
        const float pulse = 0.5f + 0.5f * std::sin(t);

        const VkRenderingAttachmentInfo colorAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = m_SwapchainViews[imageIndex],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue{.color{{0.0f, pulse, pulse * 0.5f, 1.0f}}}};

        const VkRenderingInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            // The stored extent, never the window: re-deriving it invites drift.
            .renderArea{.offset{0, 0}, .extent = m_SwapchainExtent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment};

        vkCmdBeginRendering(cmd, &renderingInfo);
        // Milestone 8 puts the draw here.
        vkCmdEndRendering(cmd);

        // give it back
        ImageBarrier(cmd, m_SwapchainImages[imageIndex],
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_NONE, 0);
    }

    void VulkanRenderer::DrawFrame()
    {
        if (m_Window->Width() == 0 || m_Window->Height() == 0)
            return;

        // check swapchain creation
        if (m_NeedRecreate || m_Window->Height() != m_SwapchainExtent.height || m_Window->Width() != m_SwapchainExtent.width)
        {
            if (!RecreateSwapchain())
                return;
        }

        const uint64_t signalValue = m_NextSignalValue++;
        const uint64_t waitValue = signalValue - kFramesInFlight;

        const VkSemaphoreWaitInfo waitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &m_Timeline,
            .pValues = &waitValue};

        // wait for next image to comeback from framesin flight
        vkWaitSemaphores(m_Device, &waitInfo, UINT64_MAX);

        // Whole-pool reset, not per-buffer.
        vkResetCommandPool(m_Device, m_CmdPools[m_FrameIndex], 0);

        uint32_t imageIndex = 0;
        const VkResult acquireResult = vkAcquireNextImageKHR(
            m_Device, m_Swapchain, UINT64_MAX,
            m_ImageAcquired[m_FrameIndex], VK_NULL_HANDLE, &imageIndex);

        // The semaphore was not signalled, so this frame cannot proceed.
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // Roll the signal value back: nothing will signal it.
            m_NeedRecreate = true;
            --m_NextSignalValue;
            return;
        }

        // Signalled and usable. Finish the frame, recreate next one.
        if (acquireResult == VK_SUBOPTIMAL_KHR)
        {
            m_NeedRecreate = true;
        }
        else if (acquireResult != VK_SUCCESS)
        {
            MTS_LOG_ERROR("vkAcquireNextImageKHR failed: {}", static_cast<int>(acquireResult));
            --m_NextSignalValue;
            return;
        }

        VkCommandBuffer cmd = m_CmdBuffers[m_FrameIndex];

        const VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

        vkBeginCommandBuffer(cmd, &beginInfo);
        RecordCommands(cmd, imageIndex);
        vkEndCommandBuffer(cmd);

        const VkSemaphoreSubmitInfo waitSem{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = m_ImageAcquired[m_FrameIndex],
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};

        const VkSemaphoreSubmitInfo signalSems[]{
            {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
             .semaphore = m_RenderComplete[imageIndex],
             .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT},
            {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
             .semaphore = m_Timeline,
             .value = signalValue,
             .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}};

        const VkCommandBufferSubmitInfo cmdInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd};

        const VkSubmitInfo2 submit{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitSem,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmdInfo,
            .signalSemaphoreInfoCount = 2,
            .pSignalSemaphoreInfos = signalSems};

        if (vkQueueSubmit2(m_GfxQueue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
        {
            MTS_LOG_ERROR("vkQueueSubmit2 failed");
            return;
        }
        const VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_RenderComplete[imageIndex],
            .swapchainCount = 1,
            .pSwapchains = &m_Swapchain,
            .pImageIndices = &imageIndex};

        const VkResult presentResult = vkQueuePresentKHR(m_GfxQueue, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
            m_NeedRecreate = true;
        else if (presentResult != VK_SUCCESS)
            MTS_LOG_ERROR("vkQueuePresentKHR failed: {}", static_cast<int>(presentResult));

        m_FrameIndex = (m_FrameIndex + 1) % kFramesInFlight;
    }
}
