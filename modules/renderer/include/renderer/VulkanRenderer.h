/**
 * @file VulkanRenderer.h
 * @author Sumin Park
 * @brief Renderer with all Vulkan signatures.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <vulkan/vulkan.h>

namespace mts
{
    class VulkanRenderer
    {
    public:
        VulkanRenderer() = default;

    private:
        bool Initialize(void *window);
        bool CreateVulkanInstance();
        VkPhysicalDevice FindPhysicalDevice();
        bool CreateDevice(VkPhysicalDevice physicalDevice);

    private:
        constexpr static uint32_t VulkanVersion{VK_API_VERSION_1_4};
        constexpr static uint32_t MaxFramesInFlight{2};

        void *m_Window = nullptr;

        VkInstance m_VulkanInstance = nullptr;
        VkPhysicalDevice m_PhysicalDevice = nullptr;
        VkDevice m_Device = nullptr;
    };
}