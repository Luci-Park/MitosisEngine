/**
 * @file GLFWWin32.cpp
 * @author Sumin Park
 * @brief Win32 specifics for GLFW
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */

#include "GLFWWindow.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace mts
{
    NativeWindowHandle GLFWWindow::NativeWindow() const
    {
        return {WindowBackend::Win32, nullptr, glfwGetWin32Window(m_handle)};
    }
}