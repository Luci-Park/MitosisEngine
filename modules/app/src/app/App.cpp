#include <app/App.h>

#include <core/fs/Paths.h>
#include <core/log/Log.h>

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
        m_desc = desc;

        WindowDesc windowDesc{};
        windowDesc.m_width = desc.m_width;
        windowDesc.m_height = desc.m_height;
        windowDesc.m_title = desc.m_title;

        m_window = Window::Create(windowDesc);
        if (!m_window)
        {
            MTS_LOG_ERROR("Window creation failed");
            return false;
        }

        if (!m_renderer.Initialize({.window = m_window.get(),
                                    .appName = desc.m_appName,
                                    .enableValidation = desc.m_enableValidation}))
        {
            MTS_LOG_ERROR("Renderer initialization failed");
            m_window.reset();
            return false;
        }

        m_initialized = true;
        return true;
    }

    AssetCache *App::Assets()
    {
        if (m_assetCache.has_value())
            return &*m_assetCache;

        if (m_assetLoadFailed)
            return nullptr; // already tried and logged; do not re-stat the disk every call

        const std::filesystem::path manifestPath = CookedAssetsDir() / "manifest.blob";
        m_assetManifest = AssetManifest::LoadFile(manifestPath);
        if (!m_assetManifest.has_value())
        {
            MTS_LOG_ERROR("Asset manifest load failed, assets unavailable: {}", manifestPath.string());
            m_assetLoadFailed = true;
            return nullptr;
        }

        // after the manifest is engaged, never before: the cache stores a raw
        // pointer to it
        m_assetCache.emplace(&*m_assetManifest, CookedAssetsDir());
        return &*m_assetCache;
    }

    SystemContext App::MakeContext(float dt)
    {
        return SystemContext{m_world, m_commands, dt, m_elapsed, m_frame};
    }

    void App::Run()
    {
        if (!m_initialized)
        {
            return;
        }

        SystemContext startContext = MakeContext(0.0f);
        m_scheduler.Start(startContext);

        auto previous = std::chrono::steady_clock::now();

        while (!m_window->ShouldClose())
        {
            m_window->PollEvents();

            const auto now = std::chrono::steady_clock::now();
            const float dt = std::min(std::chrono::duration<float>(now - previous).count(),
                                      m_desc.m_maxDeltaSeconds);
            previous = now;
            m_elapsed += dt;

            SystemContext context = MakeContext(dt);
            m_scheduler.Update(context);

            // The Render phase is reserved but empty: a render system would need
            // to reach the renderer, and engine_core must not link engine_renderer.
            m_renderer.DrawFrame();

            ++m_frame;
        }
    }

    void App::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        // let all the systems stop first
        SystemContext stopContext = MakeContext(0.0f);
        m_scheduler.Stop(stopContext);

        // Cache before manifest: the cache points at the manifest, and Initialize
        // may be called again afterwards. Leaving the cache engaged over a
        // destroyed manifest would leave a dangling pointer behind.
        m_assetCache.reset();
        m_assetManifest.reset();
        m_assetLoadFailed = false;

        m_renderer.Shutdown();
        // Renderer holds the surface built from the window: window dies last.
        m_window.reset();
        m_initialized = false;
        MTS_LOG_INFO("App shut down");
    }
}
