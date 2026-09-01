#include <app/App.h>

#include <core/fs/Paths.h>
#include <core/log/Log.h>

namespace mts
{
    App::~App()
    {
        Shutdown();
    }

    bool App::Initialize(const AppDesc &desc)
    {
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

        const std::filesystem::path manifestPath = CookedAssetsDir() / "manifest.blob";
        m_assetManifest = AssetManifest::LoadFile(manifestPath);
        if (!m_assetManifest.has_value())
        {
            MTS_LOG_ERROR("Asset manifest load failed: {}", manifestPath.string());
            m_renderer.Shutdown();
            m_window.reset();
            return false;
        }
        m_assetCache.emplace(&*m_assetManifest, CookedAssetsDir());

        m_initialized = true;
        return true;
    }

    void App::Run()
    {
        if (!m_initialized)
        {
            return;
        }

        while (!m_window->ShouldClose())
        {
            m_window->PollEvents();
            m_renderer.DrawFrame();
        }
    }

    void App::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        m_renderer.Shutdown();
        // Renderer holds the surface built from the window: window dies last.
        m_window.reset();
        m_initialized = false;
        MTS_LOG_INFO("App shut down");
    }
}
