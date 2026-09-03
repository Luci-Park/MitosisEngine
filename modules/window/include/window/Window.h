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
        uint32_t mWidth = 1280;
        uint32_t mHeight = 720;
        const char *mTitle = "MitosisEngine";
        bool mResizable = true;
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

        virtual void *NativeHandleForImGui() const { return nullptr; }

        /// 1.0 at 96 DPI, scaling up with the monitor's content scale.
        virtual float ContentScale() const { return 1.0f; }

        static std::unique_ptr<Window> Create(const WindowDesc &desc);

    protected:
        Window() = default;
    };
};
