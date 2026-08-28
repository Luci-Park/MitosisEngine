/**
 * @file VulkanRenderer.cpp
 * @author Sumin Park
 * @brief
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include "renderer/VulkanRenderer.h"
#include <core/log/Log.h>

#include <volk.h>
#include <GLFW/glfw3.h>

#include <vector>
namespace mts
{
    bool VulkanRenderer::Initialize(void *window)
    {
        m_Window = window;

        if (!CreateVulkanInstance())
        {
            MTS_LOG_CRITICAL("Vulkan Instance creation failed");
            return false;
        }

        if (m_PhysicalDevice = FindPhysicalDevice(); !m_PhysicalDevice)
        {
            MTS_LOG_CRITICAL("Failed to find an appropriate Physical device");
            return false;
        }

        if (!CreateDevice(m_PhysicalDevice))
        {
            MTS_LOG_CRITICAL("Logical GPU device creation failed");
            return false;
        }
    }

    bool VulkanRenderer::CreateVulkanInstance()
    {
        // 1. using volk? always start with volk
        if (volkInitialize() != VK_SUCCESS)
        {
            MTS_LOG_CRITICAL("Volk initialization failed");
            return false;
        }

        // 2. vulkan application instance
        VkApplicationInfo appInfo{
            // vk's type erasure method
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            // TODO: read it from somewhere so it's synced to the window
            .pApplicationName = "MitosisEngine - Window Test",
            .apiVersion = VulkanVersion};

        // 3. get extensions from window
        // TODO: sync window
        uint32_t instExtCount = 0;

        const char *const *extensions = glfwGetRequiredInstanceExtensions(&instExtCount);

        std::vector<const char *> requestedLayers{
            "VK_LAYER_KHRONOS_validation"};

        // 4. instance creation
        VkInstanceCreateInfo instCreateInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requestedLayers.size()),
            .ppEnabledLayerNames = requestedLayers.data(),
            .enabledExtensionCount = instExtCount,
            .ppEnabledExtensionNames = extensions};

        if (vkCreateInstance(&instCreateInfo, nullptr, &m_VulkanInstance) != VK_SUCCESS)
        {
            return false;
        }

        volkLoadInstance(m_VulkanInstance);
        return true;
    }

    VkPhysicalDevice VulkanRenderer::FindPhysicalDevice()
    {
        // enumerate all physical device
        // once to get count, once to fill data
        uint32_t physDeviceCount = 0;
        vkEnumeratePhysicalDevices(m_VulkanInstance, &physDeviceCount, nullptr);
        std::vector<VkPhysicalDevice> physicalDevices(physDeviceCount);
        vkEnumeratePhysicalDevices(m_VulkanInstance, &physDeviceCount, physicalDevices.data());

        VkPhysicalDevice physicalDevice = nullptr;
        if (physDeviceCount)
        {
            physicalDevice = physicalDevices[0];

            for (auto &pDev : physicalDevices)
            {
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(pDev, &props);
                if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                {
                    physicalDevice = pDev;
                    break;
                }
            }
        }

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    }
    bool VulkanRenderer::CreateDevice(VkPhysicalDevice physicalDevice)
    {
    }
}
