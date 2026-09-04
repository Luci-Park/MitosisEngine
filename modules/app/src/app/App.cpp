#include <app/App.h>

#include <core/ecs/ComponentRegistry.h>
#include <core/ecs/DeferredAccess.h>
#include <core/ecs/TransformHierarchy.h>
#include <core/fs/Paths.h>
#include <core/log/Log.h>
#include <renderer/ComponentRegistration.h>
#include <renderer/RenderSystem.h>
#include <scene/SceneIO.h>

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

        if (!mEditor.Initialize(*mWindow, mRenderer))
        {
            MTS_LOG_ERROR("Editor initialization failed");
            mRenderer.Shutdown();
            mWindow.reset();
            return false;
        }

        mInitialized = true;

        // scene graph + install destroy hook
        InstallHierarchy(mWorld);

        // Before anything may load a script
        RegisterCoreComponents();
        RegisterRendererComponents();

        // defers structural change
        mWorld.EmplaceResource<FrameCommands>(FrameCommands{&mCommands});

        // should be before any other system in PostUpdate
        mScheduler.Add<TransformPropagateSystem>(SystemPhase::PostUpdate);

        // Render runs after PostUpdate, so every WorldTransform it reads is
        // already current for this frame - see RenderSystem's own comment.
        mScheduler.Add<RenderSystem>(SystemPhase::Render, mRenderer);

        mScene = mts::NewScene("untitled");

        return true;
    }

    void App::NewScene(std::string name)
    {
        UnloadScene(mWorld, mScene);
        mScene = mts::NewScene(std::move(name));
    }

    bool App::SaveScene()
    {
        return mts::SaveScene(mWorld, mDesc.mSceneDir, mScene);
    }

    bool App::LoadScene()
    {
        // Checked here, not left to LoadScene: this must not touch mWorld or
        // mScene at all when there is nothing to load, so a Load click with
        // mSceneDir pointing nowhere leaves the current scene exactly as it
        // was rather than replacing it with an empty one.
        if (!std::filesystem::exists(mDesc.mSceneDir / "scene.json"))
        {
            MTS_LOG_ERROR("App::LoadScene: no scene.json in '{}'", mDesc.mSceneDir.string());
            return false;
        }

        UnloadScene(mWorld, mScene);
        mScene = mts::LoadScene(mWorld, mDesc.mSceneDir);
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

            mEditor.BeginFrame();

            if (mWindow->Width() != 0 && mWindow->Height() != 0)
            {
                switch (mEditor.DrawLayout(mDesc.mEnableEditorLayout, mDesc.mShowImGuiDemo))
                {
                case SceneMenuAction::New:
                    NewScene();
                    break;
                case SceneMenuAction::Save:
                    SaveScene();
                    break;
                case SceneMenuAction::Load:
                    LoadScene();
                    break;
                case SceneMenuAction::None:
                    break;
                }
            }

            // RenderSystem calls VulkanRenderer::DrawFrame from inside
            // Update (SystemPhase::Render), so this frame's draw data has to
            // be handed to the renderer before Update runs, not after.
            mRenderer.SetImGuiDrawData(mEditor.EndFrame());
            mRenderer.SetSceneViewport(mEditor.SceneViewportRect());

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

        // Reverse of Initialize: Editor's Vulkan backend needs mDevice still
        // alive, so it goes before mRenderer.Shutdown(); its GLFW backend
        // needs mWindow still alive, so it goes before mWindow.reset().
        mEditor.Shutdown(mRenderer);

        mRenderer.Shutdown();
        // Renderer holds the surface built from the window: window dies last.
        mWindow.reset();
        mInitialized = false;
        MTS_LOG_INFO("App shut down");
    }
}
