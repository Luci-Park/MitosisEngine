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
#include <editor/Editor.h>
#include <renderer/VulkanRenderer.h>
#include <scene/SceneAsset.h>
#include <window/Window.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

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

        // temp default path for scenes
        std::filesystem::path mSceneDir = "scenes/default";
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

        /// Temporary seam
        VulkanRenderer &Renderer() { return mRenderer; }

        /// The asset cache, loading the manifest on first use.
        /// Returns nullptr when no manifest is available - a build with no cooked
        /// assets, or an exe moved away from its cooked/ folder - so a missing
        /// manifest costs the caller an asset, not the whole application.
        /// The failure is sticky: the manifest is not retried every frame.
        AssetCache *Assets();

        /// The scene currently in the World - always populated after
        /// Initialize (an empty NewScene, if nothing else). A caller building
        /// a scene by hand (main.cpp's BuildScene, today) should register
        /// each entity it creates through CreateSceneEntity(GetWorld(),
        /// Scene(), ...) so File > Save Scene actually captures it.
        LoadedScene &Scene() { return mScene; }

        /// Discards mScene's entities (UnloadScene) and replaces it with an
        /// empty one. Does not touch mSceneDir - the next Save writes there.
        void NewScene(std::string name = "untitled");

        /// Writes Scene() to mSceneDir. False on I/O failure (see SaveScene).
        bool SaveScene();

        /// Discards mScene's entities and replaces it with mSceneDir's
        /// contents. False (leaving the prior scene in place) if mSceneDir
        /// has no scene.json to load.
        bool LoadScene();

    private:
        // context is rebuilt per tick
        SystemContext MakeContext(float dt);

        std::unique_ptr<Window> mWindow;
        VulkanRenderer mRenderer;
        Editor mEditor;

        // Before mWorld, so it is destroyed after it. mWorld holds a
        // FrameCommands resource pointing here, and reverse-order destruction
        // would otherwise leave that pointer dangling for the whole of ~World -
        // which tears down resources and could reach a destroy hook.
        CommandBuffer mCommands;

        World mWorld;
        SystemScheduler mScheduler;
        LoadedScene mScene;

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
    };
}
