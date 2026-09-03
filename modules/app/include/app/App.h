/**
 * @file App.h
 * @author Sumin Park
 * @brief Owns the engine subsystems and drives the main loop.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <core/ecs/CommandBuffer.h>
#include <core/ecs/SystemScheduler.h>
#include <core/ecs/World.h>
#include <assets/AssetCache.h>
#include <assets/AssetManifest.h>
#include <renderer/VulkanRenderer.h>
#include <window/Window.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace mts
{
    struct AppDesc
    {
        uint32_t mWidth = 1280;
        uint32_t mHeight = 720;
        const char *mTitle = "MitosisEngine";
        const char *mAppName = "MitosisEngine";
        bool mEnableValidation = true;

#ifdef NDEBUG
        bool mShowImGuiDemo = false;
#else
        bool mShowImGuiDemo = true;
#endif
        bool mEnableEditorLayout = true;

        float mMaxDeltaSeconds = 0.25f;
    };

    class App
    {
    public:
        App() = default;
        ~App();

        App(const App &) = delete;
        App &operator=(const App &) = delete;
        App(App &&) = delete;
        App &operator=(App &&) = delete;

        bool Initialize(const AppDesc &desc);
        void Run();
        void Shutdown();

        World &GetWorld() { return mWorld; }
        SystemScheduler &Systems() { return mScheduler; }

        /// Temporary seam: CreateMesh has nowhere else to be called from until
        /// an asset-facing mesh service exists. A caller building a scene
        /// reaches the renderer through here, uploads geometry once at load
        /// time, and stores the returned MeshHandle in a MeshRenderer.
        VulkanRenderer &Renderer() { return mRenderer; }

        /// The asset cache, loading the manifest on first use.
        /// Returns nullptr when no manifest is available - a build with no cooked
        /// assets, or an exe moved away from its cooked/ folder - so a missing
        /// manifest costs the caller an asset, not the whole application.
        /// The failure is sticky: the manifest is not retried every frame.
        AssetCache *Assets();

    private:
        // context is rebuilt per tick
        SystemContext MakeContext(float dt);

        /// Dockspace, the default Scene/Inspector/Output split, and the
        /// Debug menu - the Slate editor shell, not a generic App concern.
        /// Gated separately from the ImGui context itself by mEnableEditorLayout
        /// so a non-editor consumer of App can keep ImGui without this layout.
        void DrawEditorUI();

        std::unique_ptr<Window> mWindow;
        VulkanRenderer mRenderer;

        // Before mWorld, so it is destroyed after it. mWorld holds a
        // FrameCommands resource pointing here, and reverse-order destruction
        // would otherwise leave that pointer dangling for the whole of ~World -
        // which tears down resources and could reach a destroy hook.
        CommandBuffer mCommands;

        World mWorld;
        SystemScheduler mScheduler;

        AppDesc mDesc;
        double mElapsed = 0.0;
        uint64_t mFrame = 0;
        // mAssetCache holds a raw pointer into mAssetManifest, so the two are
        // created and torn down together, cache first. Declared in this order so
        // destruction (reverse of declaration) also destroys the cache first.
        std::optional<AssetManifest> mAssetManifest;
        std::optional<AssetCache> mAssetCache;
        bool mAssetLoadFailed = false;
        bool mInitialized = false;
        bool mImGuiInitialized = false;
        bool mShowStyleEditor = false;
        std::string mImGuiIniPath;
    };
}
