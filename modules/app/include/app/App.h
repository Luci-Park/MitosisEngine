/**
 * @file App.h
 * @author Sumin Park
 * @brief Owns the engine subsystems and drives the main loop.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <renderer/VulkanRenderer.h>
#include <window/Window.h>

#include <cstdint>
#include <memory>

namespace mts
{
    struct AppDesc
    {
        uint32_t m_width = 1280;
        uint32_t m_height = 720;
        const char *m_title = "MitosisEngine";
        const char *m_appName = "MitosisEngine";
        bool m_enableValidation = true;
    };

    /// Fixed-member composition root: no generic system registry until one is
    /// actually needed (see architecture decisions).
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

    private:
        std::unique_ptr<Window> m_window;
        VulkanRenderer m_renderer;
        bool m_initialized = false;
    };
}
