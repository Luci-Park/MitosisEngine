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
#include <core/ecs/components/TriangleRenderer.h>
#include <core/ecs/components/WorldTransform.h>
#include <assets/AssetCache.h>
#include <assets/AssetManifest.h>
#include <renderer/VulkanRenderer.h>
#include <window/Window.h>

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace mts
{
    struct AppDesc
    {
        uint32_t mWidth = 1280;
        uint32_t mHeight = 720;
        const char *mTitle = "MitosisEngine";
        const char *mAppName = "MitosisEngine";
        bool mEnableValidation = true;

        bool mShowImGuiDemo = true;

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

        /// The asset cache, loading the manifest on first use.
        /// Returns nullptr when no manifest is available - a build with no cooked
        /// assets, or an exe moved away from its cooked/ folder - so a missing
        /// manifest costs the caller an asset, not the whole application.
        /// The failure is sticky: the manifest is not retried every frame.
        AssetCache *Assets();

    private:
        // context is rebuilt per tick
        SystemContext MakeContext(float dt);

        /// Refills mDrawInstances from the world. This is the whole ECS ->
        /// renderer bridge, and it lives here rather than in either module:
        /// engine_core must not link engine_renderer, and the renderer takes
        /// plain matrices so it never learns what a World is. App links both,
        /// so App is the only place the two may meet.
        /// Refills mDrawInstances from the world :
        void CollectDrawInstances();

        std::unique_ptr<Window> mWindow;
        VulkanRenderer mRenderer;

        // Before mWorld, so it is destroyed after it. mWorld holds a
        // FrameCommands resource pointing here, and reverse-order destruction
        // would otherwise leave that pointer dangling for the whole of ~World -
        // which tears down resources and could reach a destroy hook. See
        // decision 0019 on releasing handles before the world goes down.
        CommandBuffer mCommands;

        World mWorld;
        SystemScheduler mScheduler;

        /// Rebuilt every frame but keeps its capacity to steady allocation.
        std::vector<glm::mat4> mDrawInstances;

        Query<const WorldTransform> *mDrawQuery = nullptr;

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
    };
}
