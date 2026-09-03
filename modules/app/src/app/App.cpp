#include <app/App.h>

#include <core/ecs/ComponentRegistry.h>
#include <core/ecs/DeferredAccess.h>
#include <core/ecs/TransformHierarchy.h>
#include <core/fs/Paths.h>
#include <core/log/Log.h>
#include <renderer/ComponentRegistration.h>
#include <renderer/RenderSystem.h>

#include <algorithm>
#include <chrono>

namespace mts
{
    App::~App()
    {
        Shutdown();
    }

    bool App::Initialize(const AppDesc &desc)
    {
        mDesc = desc;

        WindowDesc windowDesc{};
        windowDesc.mWidth = desc.mWidth;
        windowDesc.mHeight = desc.mHeight;
        windowDesc.mTitle = desc.mTitle;

        mWindow = Window::Create(windowDesc);
        if (!mWindow)
        {
            MTS_LOG_ERROR("Window creation failed");
            return false;
        }

        if (!mRenderer.Initialize({.window = mWindow.get(),
                                   .appName = desc.mAppName,
                                   .enableValidation = desc.mEnableValidation}))
        {
            MTS_LOG_ERROR("Renderer initialization failed");
            mWindow.reset();
            return false;
        }

        // Destruction needs no system: InstallHierarchy puts the scene graph
        // in place and arms the destroy hook, so World::DestroyEntity cascades
        // and nothing is ever left orphaned for a pass to reap. Called here so
        // a world destroyed into before its first AddTransform still cascades.
        InstallHierarchy(mWorld);

        // Before anything may load a script: the registry gives a name to
        // whoever claims it first, and a script-declared component that stole
        // "Transform" would be refused here rather than at its own callsite.
        RegisterCoreComponents();
        RegisterRendererComponents();

        // Publishes the frame's buffer so a caller with only a World - a script
        // binding, an editor command - can defer a structural change it is not
        // allowed to make immediately. The scheduler already flushes this
        // buffer at every phase boundary; a second one would be flushed by
        // nobody, which is why the resource holds a pointer.
        mWorld.EmplaceResource<FrameCommands>(FrameCommands{&mCommands});

        // should be before any other system in PostUpdate
        mScheduler.Add<TransformPropagateSystem>(SystemPhase::PostUpdate);

        // Render runs after PostUpdate, so every WorldTransform it reads is
        // already current for this frame - see RenderSystem's own comment.
        mScheduler.Add<RenderSystem>(SystemPhase::Render, mRenderer);

        mInitialized = true;
        return true;
    }

    AssetCache *App::Assets()
    {
        if (mAssetCache.has_value())
            return &*mAssetCache;

        if (mAssetLoadFailed)
            return nullptr; // already tried and logged; do not re-stat the disk every call

        const std::filesystem::path manifestPath = CookedAssetsDir() / "manifest.blob";
        mAssetManifest = AssetManifest::LoadFile(manifestPath);
        if (!mAssetManifest.has_value())
        {
            MTS_LOG_ERROR("Asset manifest load failed, assets unavailable: {}", manifestPath.string());
            mAssetLoadFailed = true;
            return nullptr;
        }

        // after the manifest is engaged, never before: the cache stores a raw
        // pointer to it
        mAssetCache.emplace(&*mAssetManifest, CookedAssetsDir());
        return &*mAssetCache;
    }

    SystemContext App::MakeContext(float dt)
    {
        return SystemContext{mWorld, mCommands, dt, mElapsed, mFrame};
    }

    void App::Run()
    {
        if (!mInitialized)
        {
            return;
        }

        SystemContext startContext = MakeContext(0.0f);
        mScheduler.Start(startContext);

        auto previous = std::chrono::steady_clock::now();

        while (!mWindow->ShouldClose())
        {
            mWindow->PollEvents();

            const auto now = std::chrono::steady_clock::now();
            const float dt = std::min(std::chrono::duration<float>(now - previous).count(),
                                      mDesc.mMaxDeltaSeconds);
            previous = now;
            mElapsed += dt;

            SystemContext context = MakeContext(dt);
            mScheduler.Update(context);

            ++mFrame;
        }
    }

    void App::Shutdown()
    {
        if (!mInitialized)
        {
            return;
        }

        // let all the systems stop first
        SystemContext stopContext = MakeContext(0.0f);
        mScheduler.Stop(stopContext);

        // Dropped, not kept: Initialize may run again (see below), and it
        // registers TransformPropagateSystem unconditionally. Keeping the old
        // list would run a second copy of it, and of every game system, on
        // every frame of the next session.
        mScheduler.Reset();

        // Cache before manifest: the cache points at the manifest, and Initialize
        // may be called again afterwards. Leaving the cache engaged over a
        // destroyed manifest would leave a dangling pointer behind.
        mAssetCache.reset();
        mAssetManifest.reset();
        mAssetLoadFailed = false;

        mRenderer.Shutdown();
        // Renderer holds the surface built from the window: window dies last.
        mWindow.reset();
        mInitialized = false;
        MTS_LOG_INFO("App shut down");
    }
}
