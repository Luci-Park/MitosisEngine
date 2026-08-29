/**
 * @file Window.h
 * @author Sumin Park
 * @brief The interface representing the window
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <core/platform/Surface.h>

#include <cstdint>
#include <memory>

namespace mts
{
    struct WindowDesc
    {
        uint32_t m_width = 1280;
        uint32_t m_height = 720;
        const char *m_title = "MitosisEngine";
        bool m_resizable = true;
    };

    class Window : public ISurfaceProvider
    {
    public:
        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;
        Window(Window &&) = delete;
        Window &operator=(Window &&) = delete;

        virtual void PollEvents() = 0;
        virtual bool ShouldClose() const = 0;

        static std::unique_ptr<Window> Create(const WindowDesc &desc);

    protected:
        Window() = default;
    };
};
