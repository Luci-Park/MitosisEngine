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

namespace mts
{
    struct AppDesc
    {
        uint32_t m_width = 1280;
        uint32_t m_height = 720;
        const char *m_title = "MitosisEngine";
        const char *m_appName = "MitosisEngine";
        bool m_enableValidation = true;

        /// A stalled frame (breakpoint, window drag) would otherwise hand systems
        /// a multi-second dt and teleport everything.
        float m_maxDeltaSeconds = 0.25f;
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

        World &GetWorld() { return m_world; }
        SystemScheduler &Systems() { return m_scheduler; }

    private:
        // context is rebuilt per tick
        SystemContext MakeContext(float dt);

        std::unique_ptr<Window> m_window;
        VulkanRenderer m_renderer;

        World m_world;
        CommandBuffer m_commands;
        SystemScheduler m_scheduler;

        AppDesc m_desc;
        double m_elapsed = 0.0;
        uint64_t m_frame = 0;
        std::optional<AssetManifest> m_assetManifest;
        std::optional<AssetCache> m_assetCache;
        bool m_initialized = false;
    };
}
