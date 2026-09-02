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

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <span>
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

        void DrawFrame(std::span<const glm::mat4> instances);

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
        void RecordCommands(VkCommandBuffer cmd, uint32_t imageIndex, std::span<const glm::mat4> instances);

    private:
        constexpr static uint32_t VulkanVersion{VK_API_VERSION_1_3};
        constexpr static uint32_t kFramesInFlight = 2;

        const ISurfaceProvider *mWindow;
        VkInstance mVulkanInstance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;

        VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
        VkDevice mDevice = VK_NULL_HANDLE;
        VkQueue mGfxQueue = VK_NULL_HANDLE;
        VmaAllocator mAllocator = VK_NULL_HANDLE;

        VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
        VkFormat mSwapchainFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D mSwapchainExtent{};
        std::vector<VkImage> mSwapchainImages;
        std::vector<VkImageView> mSwapchainViews;

        // queue for render + present
        uint32_t mGfxQueueFamIdx = UINT32_MAX;

        VkCommandPool mCmdPools[kFramesInFlight]{};
        VkCommandBuffer mCmdBuffers[kFramesInFlight]{};
        VkSemaphore mImageAcquired[kFramesInFlight]{};

        std::vector<VkSemaphore> mRenderComplete;
        VkSemaphore mTimeline = VK_NULL_HANDLE;

        // waitvalue = signalValue - kFramesInFlight
        // dont start at 0 or 1 or else underflows
        uint64_t mNextSignalValue = kFramesInFlight + 1;

        VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
        VkPipeline mPipeline = VK_NULL_HANDLE;

        VkBuffer mVertexBuffer = VK_NULL_HANDLE;
        VmaAllocation mVertexBufferAllocation = VK_NULL_HANDLE;

        uint32_t mFrameIndex = 0;
        bool mNeedRecreate = false;
        bool mValidationEnabled = false;
    };
}
