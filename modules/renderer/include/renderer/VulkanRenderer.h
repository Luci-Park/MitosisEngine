/**
 * @file VulkanRenderer.h
 * @author Sumin Park
 * @brief Renderer with all Vulkan signatures.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <window/Window.h>

#include <volk.h>

#include <cstdint>

namespace mts
{
    struct RendererDesc
    {
        const Window *window;
        const char *appName;
        bool enableValidation;
    };

    class VulkanRenderer
    {
    public:
        VulkanRenderer() = default;
        bool Initialize(const RendererDesc &desc);
        void Shutdown();

    private:
        bool CreateVulkanInstance(const RendererDesc &desc);
        bool CreateDebugMessenger();
        bool CreateSurface();
        bool FindPhysicalDevice();

    private:
        constexpr static uint32_t VulkanVersion{VK_API_VERSION_1_3};
        const Window *m_Window;
        VkInstance m_VulkanInstance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLEE;

        VkPhysicalDevice m_PphysicalDevice = VK_NULL_HANDLE;

        // render + present
        uint32_t m_GfxQueueFamIdx = UINT32_MAX;

        bool m_ValidationEnabled = false;
    };
}
