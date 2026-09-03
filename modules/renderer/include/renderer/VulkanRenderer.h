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
#include <renderer/Mesh.h>

#include <volk.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

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

    /// One draw call's worth of data, built by RenderSystem from a
    /// WorldTransform + MeshRenderer pair.
    struct DrawItem
    {
        MeshHandle mesh;

        /// model * view projection
        glm::mat4 model{1.0f};

        glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
    };

    class VulkanRenderer
    {
    public:
        VulkanRenderer() = default;
        bool Initialize(const RendererDesc &desc);

        // Uploads geometry and returns a handle to it.
        // Use on load time
        MeshHandle CreateMesh(std::span<const Vertex> vertices,
                              std::span<const uint32_t> indices);

        /// Width/height of the current swapchain
        float AspectRatio() const
        {
            return mSwapchainExtent.height == 0
                       ? 1.0f
                       : static_cast<float>(mSwapchainExtent.width) / static_cast<float>(mSwapchainExtent.height);
        }

        void DrawFrame(std::span<const DrawItem> items);

        void SetClearColor(const glm::vec4 &color) { mClearColor = color; }

        void Shutdown();

    private:
        struct GpuMesh;

        bool CreateVulkanInstance(const RendererDesc &desc);
        bool CreateDebugMessenger();
        bool CreateSurface();
        bool FindPhysicalDevice();
        bool CreateLogicalDevice();
        bool CreateAllocator();
        bool CreateSwapchain();
        bool CreateImageViews();
        bool CreateDepthResources();
        void DestroyDepthResources();
        void DestroySwapchain();
        bool RecreateSwapchain();
        bool CreateFrameResources();
        bool CreateRenderCompleteSemaphores();
        void DestroyRenderCompleteSemaphores();
        bool CreateGraphicsPipeline();
        void DestroyMeshes();
        const GpuMesh *FindMesh(MeshHandle handle) const;
        void NameObject(VkObjectType type, uint64_t handle, const char *name);
        void RecordCommands(VkCommandBuffer cmd, uint32_t imageIndex, std::span<const DrawItem> items);

    private:
        constexpr static uint32_t VulkanVersion{VK_API_VERSION_1_3};
        constexpr static uint32_t kFramesInFlight = 2;

        // 32-bit float depth, no stencil.
        constexpr static VkFormat kDepthFormat = VK_FORMAT_D32_SFLOAT;

        // Default: dark gray. Overridden via SetClearColor.
        glm::vec4 mClearColor{0.02f, 0.02f, 0.02f, 1.0f};

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

        VkImage mDepthImages[kFramesInFlight]{};
        VmaAllocation mDepthAllocations[kFramesInFlight]{};
        VkImageView mDepthViews[kFramesInFlight]{};

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

        // One buffer pair per mesh
        struct GpuMesh
        {
            VkBuffer mVertexBuffer = VK_NULL_HANDLE;
            VmaAllocation mVertexAllocation = VK_NULL_HANDLE;
            VkBuffer mIndexBuffer = VK_NULL_HANDLE;
            VmaAllocation mIndexAllocation = VK_NULL_HANDLE;
            uint32_t mIndexCount = 0;
            uint32_t mGeneration = 0; //< 0 = slot never filled
        };

        /// Indices are stable: nothing is ever erased from this vector, which
        /// is what makes MeshHandle::mIndex a plain subscript.
        std::vector<GpuMesh> mMeshes;

        uint32_t mFrameIndex = 0;
        bool mNeedRecreate = false;
        bool mValidationEnabled = false;
    };
}
