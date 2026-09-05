/**
 * @file Surface.h
 * @author Sumin Park
 * @brief Native window handle + the minimal live-surface contract renderers need.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <cstdint>
#include <format>

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

    constexpr const char *ToString(WindowBackend backend) noexcept
    {
        switch (backend)
        {
        case WindowBackend::None:    return "None";
        case WindowBackend::Win32:   return "Win32";
        case WindowBackend::Xlib:    return "Xlib";
        case WindowBackend::Wayland: return "Wayland";
        case WindowBackend::Cocoa:   return "Cocoa";
        }
        return "Unknown";
    }

    // for graphics borrowing
    struct NativeWindowHandle
    {
        WindowBackend backend = WindowBackend::None;
        void *display = nullptr; // null on Win32 & Cocoa
        void *window = nullptr;
    };

    // Minimal rect type for data crossing the window boundary without
    // pulling in a specific graphics API's headers (window links no
    // Vulkan headers - see VkRect2D usage in the editor/renderer layer).
    struct PixelRect
    {
        int32_t x = 0;
        int32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    // What a renderer needs from a window, and nothing else - no event pump.
    // Window (in the window module) implements this on top of its own interface.
    class ISurfaceProvider
    {
    public:
        virtual ~ISurfaceProvider() = default;

        virtual NativeWindowHandle NativeWindow() const = 0;
        virtual uint32_t Width() const = 0;
        virtual uint32_t Height() const = 0;
    };
}

// Lets WindowBackend be used directly in std::format / MTS_LOG_* calls.
template <>
struct std::formatter<mts::WindowBackend> : std::formatter<const char *>
{
    auto format(mts::WindowBackend backend, std::format_context &ctx) const
    {
        return std::formatter<const char *>::format(mts::ToString(backend), ctx);
    }
};
