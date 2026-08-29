/**
 * @file VulkanRenderer.h
 * @author Sumin Park
 * @brief Renderer with all Vulkan signatures.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <core/platform/Surface.h>

#include <volk.h>

#include <cstdint>
#include <vector>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace mts
{
    struct RendererDesc
    {
        const ISurfaceProvider *window;
        const char *appName;
        bool enableValidation;
    };

    class VulkanRenderer
    {
    public:
        VulkanRenderer() = default;
        bool Initialize(const RendererDesc &desc);
        void DrawFrame();
        void Shutdown();

    private:
        bool CreateVulkanInstance(const RendererDesc &desc);
        bool CreateDebugMessenger();
        bool CreateSurface();
        bool FindPhysicalDevice();
        bool CreateLogicalDevice();
        bool CreateAllocator();
        bool CreateSwapchain();
        bool CreateImageViews();
        void DestroySwapchain();
        bool RecreateSwapchain();
        bool CreateFrameResources();
        bool CreateRenderCompleteSemaphores();
        void DestroyRenderCompleteSemaphores();
        bool CreateGraphicsPipeline();
        bool CreateVertexBuffer();
        void DestroyVertexBuffer();
        void NameObject(VkObjectType type, uint64_t handle, const char *name);
        void RecordCommands(VkCommandBuffer cmd, uint32_t imageIndex);

    private:
        constexpr static uint32_t VulkanVersion{VK_API_VERSION_1_3};
        constexpr static uint32_t kFramesInFlight = 2;

        const ISurfaceProvider *m_Window;
        VkInstance m_VulkanInstance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;
        VkQueue m_GfxQueue = VK_NULL_HANDLE;
        VmaAllocator m_Allocator = VK_NULL_HANDLE;

        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        VkFormat m_SwapchainFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D m_SwapchainExtent{};
        std::vector<VkImage> m_SwapchainImages;
        std::vector<VkImageView> m_SwapchainViews;

        // queue for render + present
        uint32_t m_GfxQueueFamIdx = UINT32_MAX;

        VkCommandPool m_CmdPools[kFramesInFlight]{};
        VkCommandBuffer m_CmdBuffers[kFramesInFlight]{};
        VkSemaphore m_ImageAcquired[kFramesInFlight]{};

        std::vector<VkSemaphore> m_RenderComplete;
        VkSemaphore m_Timeline = VK_NULL_HANDLE;

        // waitvalue = signalValue - kFramesInFlight
        // dont start at 0 or 1 or else underflows
        uint64_t m_NextSignalValue = kFramesInFlight + 1;

        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;

        VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
        VmaAllocation m_VertexBufferAllocation = VK_NULL_HANDLE;

        uint32_t m_FrameIndex = 0;
        bool m_NeedRecreate = false;
        bool m_ValidationEnabled = false;
    };
}
