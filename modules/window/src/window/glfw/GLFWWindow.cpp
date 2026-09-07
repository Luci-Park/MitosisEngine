/**
 * @file GLFWWindow.cpp
 * @author Sumin Park
 * @brief GLFW window for desktop platforms
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include "GLFWWindow.h"

#include "core/log/Assert.h"
#include "core/log/Log.h"

#include <GLFW/glfw3.h>

namespace mir
{
    namespace
    {
        // GLFW is a process-wide singleton, so init/terminate is refcounted
        // against the number of live windows.
        int g_windowCount = 0;

        void OnGLFWError(int code, const char *description)
        {
            MIR_LOG_ERROR("GLFW error {}: {}", code, description);
        }

        static_assert(static_cast<int>(Key::Space) == GLFW_KEY_SPACE);
        static_assert(static_cast<int>(Key::A) == GLFW_KEY_A);
        static_assert(static_cast<int>(Key::Z) == GLFW_KEY_Z);
        static_assert(static_cast<int>(Key::Num0) == GLFW_KEY_0);
        static_assert(static_cast<int>(Key::F1) == GLFW_KEY_F1);
        static_assert(static_cast<int>(Key::F25) == GLFW_KEY_F25);
        static_assert(static_cast<int>(Key::Keypad0) == GLFW_KEY_KP_0);
        static_assert(static_cast<int>(Key::KeypadEqual) == GLFW_KEY_KP_EQUAL);
        static_assert(static_cast<int>(Key::LeftShift) == GLFW_KEY_LEFT_SHIFT);
        static_assert(static_cast<int>(Key::Menu) == GLFW_KEY_MENU);
        static_assert(kKeyCount == GLFW_KEY_LAST + 1);
        static_assert(static_cast<int>(MouseButton::Left) == GLFW_MOUSE_BUTTON_LEFT);
        static_assert(static_cast<int>(MouseButton::Button8) == GLFW_MOUSE_BUTTON_8);
        static_assert(kMouseButtonCount == GLFW_MOUSE_BUTTON_LAST + 1);
        static_assert(static_cast<int>(GamepadButton::A) == GLFW_GAMEPAD_BUTTON_A);
        static_assert(static_cast<int>(GamepadButton::DpadLeft) == GLFW_GAMEPAD_BUTTON_DPAD_LEFT);
        static_assert(kGamepadButtonCount == GLFW_GAMEPAD_BUTTON_LAST + 1);
        static_assert(static_cast<int>(GamepadAxis::LeftX) == GLFW_GAMEPAD_AXIS_LEFT_X);
        static_assert(static_cast<int>(GamepadAxis::RightTrigger) == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER);
        static_assert(kGamepadAxisCount == GLFW_GAMEPAD_AXIS_LAST + 1);
        static_assert(kGamepadSlotCount == GLFW_JOYSTICK_LAST + 1);

        constexpr Key kAllKeys[] = {
            Key::Space, Key::Apostrophe, Key::Comma, Key::Minus, Key::Period, Key::Slash,
            Key::Num0, Key::Num1, Key::Num2, Key::Num3, Key::Num4,
            Key::Num5, Key::Num6, Key::Num7, Key::Num8, Key::Num9,
            Key::Semicolon, Key::Equal,
            Key::A, Key::B, Key::C, Key::D, Key::E, Key::F, Key::G, Key::H, Key::I, Key::J,
            Key::K, Key::L, Key::M, Key::N, Key::O, Key::P, Key::Q, Key::R, Key::S, Key::T,
            Key::U, Key::V, Key::W, Key::X, Key::Y, Key::Z,
            Key::LeftBracket, Key::Backslash, Key::RightBracket, Key::GraveAccent,
            Key::Escape, Key::Enter, Key::Tab, Key::Backspace, Key::Insert, Key::Delete,
            Key::Right, Key::Left, Key::Down, Key::Up,
            Key::PageUp, Key::PageDown, Key::Home, Key::End,
            Key::CapsLock, Key::ScrollLock, Key::NumLock, Key::PrintScreen, Key::Pause,
            Key::F1, Key::F2, Key::F3, Key::F4, Key::F5, Key::F6, Key::F7, Key::F8, Key::F9, Key::F10,
            Key::F11, Key::F12, Key::F13, Key::F14, Key::F15, Key::F16, Key::F17, Key::F18, Key::F19, Key::F20,
            Key::F21, Key::F22, Key::F23, Key::F24, Key::F25,
            Key::Keypad0, Key::Keypad1, Key::Keypad2, Key::Keypad3, Key::Keypad4,
            Key::Keypad5, Key::Keypad6, Key::Keypad7, Key::Keypad8, Key::Keypad9,
            Key::KeypadDecimal, Key::KeypadDivide, Key::KeypadMultiply, Key::KeypadSubtract,
            Key::KeypadAdd, Key::KeypadEnter, Key::KeypadEqual,
            Key::LeftShift, Key::LeftControl, Key::LeftAlt, Key::LeftSuper,
            Key::RightShift, Key::RightControl, Key::RightAlt, Key::RightSuper, Key::Menu,
        };
    }

    std::unique_ptr<Window> Window::Create(const WindowDesc &desc)
    {
        return std::make_unique<GLFWWindow>(desc);
    }

    GLFWWindow::GLFWWindow(const WindowDesc &desc)
    {
        if (g_windowCount == 0)
        {
            glfwSetErrorCallback(&OnGLFWError);
            MIR_CHECK(glfwInit() == GLFW_TRUE, "glfwInit failed");
        }

        // No OpenGL context. The renderer owns the graphics API.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, desc.mResizable ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_MAXIMIZED, desc.mMaximized ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, desc.mStartHidden ? GLFW_FALSE : GLFW_TRUE);

        mHandle = glfwCreateWindow(static_cast<int>(desc.mWidth),
                                    static_cast<int>(desc.mHeight),
                                    desc.mTitle,
                                    nullptr,
                                    nullptr);
        MIR_CHECK(mHandle != nullptr, "glfwCreateWindow failed");
        ++g_windowCount;

        mTitle = desc.mTitle;
        mHasCustomTitleBar = desc.mCustomTitleBar;
        if (mHasCustomTitleBar)
            InstallCustomTitleBar();

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(mHandle, &fbWidth, &fbHeight);
        mWidth = static_cast<uint32_t>(fbWidth);
        mHeight = static_cast<uint32_t>(fbHeight);

        // Lets the static callbacks recover the owning instance.
        glfwSetWindowUserPointer(mHandle, this);
        glfwSetFramebufferSizeCallback(mHandle, &GLFWWindow::OnFramebufferSize);
        glfwSetScrollCallback(mHandle, &GLFWWindow::OnScroll);

        MIR_LOG_INFO("Window created: {}x{} \"{}\"", mWidth, mHeight, desc.mTitle);
    }

    GLFWWindow::~GLFWWindow()
    {
        if (mHandle != nullptr)
        {
            if (mHasCustomTitleBar)
                UninstallCustomTitleBar();

            glfwDestroyWindow(mHandle);
            mHandle = nullptr;
            --g_windowCount;
        }

        if (g_windowCount == 0)
        {
            glfwTerminate();
        }
    }

    void GLFWWindow::PollEvents()
    {
        mRawInput.scrollX = 0.0;
        mRawInput.scrollY = 0.0;

        glfwPollEvents();

        for (Key key : kAllKeys)
            mRawInput.keys[static_cast<size_t>(key)] = glfwGetKey(mHandle, static_cast<int>(key)) == GLFW_PRESS;

        for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST; ++button)
            mRawInput.mouseButtons[static_cast<size_t>(button)] = glfwGetMouseButton(mHandle, button) == GLFW_PRESS;

        glfwGetCursorPos(mHandle, &mRawInput.mouseX, &mRawInput.mouseY);

        for (int jid = 0; jid <= GLFW_JOYSTICK_LAST; ++jid)
        {
            GamepadState &pad = mRawInput.gamepads[static_cast<size_t>(jid)];

            GLFWgamepadstate state{};
            if (glfwGetGamepadState(jid, &state) != GLFW_TRUE)
            {
                pad.connected = false;
                continue;
            }

            pad.connected = true;
            for (int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST; ++b)
                pad.buttons[static_cast<size_t>(b)] = state.buttons[b] == GLFW_PRESS;
            for (int a = 0; a <= GLFW_GAMEPAD_AXIS_LAST; ++a)
                pad.axes[static_cast<size_t>(a)] = state.axes[a];
        }
    }

    bool GLFWWindow::ShouldClose() const
    {
        return glfwWindowShouldClose(mHandle) == GLFW_TRUE;
    }

    float GLFWWindow::ContentScale() const
    {
        float xscale = 1.0f;
        float yscale = 1.0f;
        glfwGetWindowContentScale(mHandle, &xscale, &yscale);
        return xscale;
    }

    void GLFWWindow::Minimize()
    {
        glfwIconifyWindow(mHandle);
    }

    void GLFWWindow::ToggleMaximize()
    {
        if (glfwGetWindowAttrib(mHandle, GLFW_MAXIMIZED) == GLFW_TRUE)
            glfwRestoreWindow(mHandle);
        else
            glfwMaximizeWindow(mHandle);
    }

    bool GLFWWindow::IsMaximized() const
    {
        return glfwGetWindowAttrib(mHandle, GLFW_MAXIMIZED) == GLFW_TRUE;
    }

    void GLFWWindow::RequestClose()
    {
        glfwSetWindowShouldClose(mHandle, GLFW_TRUE);
    }

    void GLFWWindow::Show()
    {
        glfwShowWindow(mHandle);
    }

    void GLFWWindow::OnFramebufferSize(GLFWwindow *handle, int width, int height)
    {
        auto *self = static_cast<GLFWWindow *>(glfwGetWindowUserPointer(handle));
        MIR_ASSERT(self != nullptr, "framebuffer callback without a user pointer");

        // Minimizing reports 0x0. Keep it; the renderer decides to skip frames.
        self->mWidth = static_cast<uint32_t>(width);
        self->mHeight = static_cast<uint32_t>(height);
    }

    void GLFWWindow::OnScroll(GLFWwindow *handle, double xoffset, double yoffset)
    {
        auto *self = static_cast<GLFWWindow *>(glfwGetWindowUserPointer(handle));
        MIR_ASSERT(self != nullptr, "scroll callback without a user pointer");

        self->mRawInput.scrollX += xoffset;
        self->mRawInput.scrollY += yoffset;
    }
}
