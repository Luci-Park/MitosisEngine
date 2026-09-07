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
#include <editor/panels/LogPanel.h>
#include <renderer/VulkanRenderer.h>
#include <window/Window.h>

#include <string>
#include <vector>

struct ImDrawData;

namespace mir
{
    enum class SceneMenuAction
    {
        None,
        New,
        Save,
        Load,
    };

    class Editor
    {
    public:
        Editor() = default;
        ~Editor();

        Editor(const Editor &) = delete;
        Editor &operator=(const Editor &) = delete;
        Editor(Editor &&) = delete;
        Editor &operator=(Editor &&) = delete;

        // `renderer`'s device and render target must already be created.
        bool Initialize(Window &window, VulkanRenderer &renderer);

        // Must run before renderer.Shutdown() and before the window is
        // destroyed. No-op when not initialized.
        void Shutdown(VulkanRenderer &renderer);

        // Call once per frame, before DrawLayout or any editor drawing.
        void BeginFrame();

        // Draws the editor shell and returns the File menu action picked this frame.
        // enableLayout = false disables editor
        SceneMenuAction DrawLayout(bool enableLayout);

        // Returns draw data for VulkanRenderer::SetImGuiDrawData.
        // BeginFrame is needed regardless of skipping layout
        ImDrawData *EndFrame();

        // The dockspace's central node, where the 3D scene shows through.
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
        LogPanel mLogPanel;
    };
}
