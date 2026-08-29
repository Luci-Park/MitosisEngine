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

#include <vector>
#include <cstring>
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
        {
            MTS_LOG_ERROR("vulkan instance creation failed");
            return false;
        }

        volkLoadInstance(m_VulkanInstance);

        if (m_ValidationEnabled && !CreateDebugMessenger())
        {
            MTS_LOG_ERROR("debug messenger creation failed");
            return false;
        }

        return true;
    }
    void VulkanRenderer::Shutdown()
    {
        if (m_DebugMessenger != VK_NULL_HANDLE)
        {
            vkDestroyDebugUtilsMessengerEXT(m_VulkanInstance, m_DebugMessenger, nullptr);
            m_DebugMessenger = VK_NULL_HANDLE;
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
}
