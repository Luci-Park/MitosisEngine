/**
 * @file Editor.h
 * @author Sumin Park
 * @brief Owns the ImGui context, backend, and the Slate editor shell
 *        (dockspace, Hierarchy/Inspector/Output panels, Debug menu).
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <core/platform/Surface.h>
#include <renderer/VulkanRenderer.h>
#include <window/Window.h>

#include <string>
#include <vector>

struct ImDrawData;

namespace mts
{
    class Editor
    {
    public:
        Editor() = default;
        ~Editor();

        Editor(const Editor &) = delete;
        Editor &operator=(const Editor &) = delete;
        Editor(Editor &&) = delete;
        Editor &operator=(Editor &&) = delete;

        /// Creates the ImGui context, loads fonts/theme, and wires up the
        /// GLFW + Vulkan backends. renderer's Vulkan backend must be ready
        /// to accept InitImGuiVulkanBackend (device/render target created).
        bool Initialize(Window &window, VulkanRenderer &renderer);

        /// Reverse of Initialize. Vulkan backend teardown needs renderer's
        /// device still alive, so this must run before renderer.Shutdown();
        /// the GLFW backend needs the window still alive, so before it is
        /// destroyed too. Safe to call when not initialized (no-op).
        void Shutdown(VulkanRenderer &renderer);

        /// Starts this frame's ImGui state. Call once per iteration before
        /// any other ImGui-facing call (including DrawLayout).
        void BeginFrame();

        /// Builds the dockspace, the default Hierarchy/Inspector/Output
        /// split, the Debug menu, and (if requested) the style editor -
        /// the Slate editor shell. Pass enableLayout = false to keep ImGui
        /// running (e.g. a caller's own UI) without this shell.
        void DrawLayout(bool enableLayout);

        /// Ends this frame's ImGui state and returns its draw data, which
        /// the caller hands to VulkanRenderer::SetImGuiDrawData. Always
        /// pair with BeginFrame, even on a frame DrawLayout was skipped -
        /// ImGui's NewFrame/Render calls must still come in pairs.
        ImDrawData *EndFrame();

        /// The dockspace's central passthru node - where the 3D scene
        /// shows through, since ImGuiDockNodeFlags_PassthruCentralNode
        /// leaves it undocked. Valid after DrawLayout(true); zero extent
        /// (the default, and what DrawLayout(false) leaves it at) means
        /// "use the full swapchain" to VulkanRenderer.
        VkRect2D SceneViewportRect() const { return mSceneViewportRect; }

        bool IsInitialized() const { return mInitialized; }

    private:
        void DrawTitleBar();

        bool mInitialized = false;
        bool mShowStyleEditor = false;
        std::string mImGuiIniPath;
        VkRect2D mSceneViewportRect{};
        Window *mWindow = nullptr;
        std::vector<PixelRect> mTitleBarInteractiveRects;
    };
}
