/**
 * @file Window.h
 * @author Sumin Park
 * @brief The interface representing the window
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <cstdint>
#include <memory>

namespace mts
{
    enum class WindowBackend
    {
        None,
        Win32,   // windows
        Xlib,    // old linux / unix
        Wayland, // modern linux
        Cocoa    // macOS
    };

    // for graphics borrowing
    struct NativeWindowHandle
    {
        WindowBackend backend = WindowBackend::None;
        void *display = nullptr; // null on Win32 & Cocoa
        void *window = nullptr;
    };

    struct WindowDesc
    {
        uint32_t m_width = 1280;
        uint32_t m_height = 720;
        const char *m_title = "MitosisEngine";
        bool m_resizable = true;
    };

    class Window
    {
    public:
        virtual ~Window() = default;

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;
        Window(Window &&) = delete;
        Window &operator=(Window &&) = delete;

        virtual void PollEvents() = 0;
        virtual bool ShouldClose() const = 0;

        virtual uint32_t Width() const = 0;
        virtual uint32_t Height() const = 0;

        virtual NativeWindowHandle NativeWindow() const = 0;

        static std::unique_ptr<Window> Create(const WindowDesc &desc);

    protected:
        Window() = default;
    };
};
