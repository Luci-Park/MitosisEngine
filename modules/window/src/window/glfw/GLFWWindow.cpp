/**
 * @file GLFWWindow.cpp
 * @author Sumin Park
 * @brief GLFW window for desktop platforms
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include "GLFWWindow.h"

#include "core/Platform.h"
#include "core/log/Assert.h"
#include "core/log/Log.h"

#include <GLFW/glfw3.h>

#if ENGINE_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#elif ENGINE_PLATFORM_MACOS
#define GLFW_EXPOSE_NATIVE_COCOA
#elif ENGINE_PLATFORM_LINUX
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>

namespace mts
{
    namespace
    {
        // GLFW is a process-wide singleton, so init/terminate is refcounted
        // against the number of live windows.
        int g_windowCount = 0;

        void OnGLFWError(int code, const char *description)
        {
            MTS_LOG_ERROR("GLFW error {}: {}", code, description);
        }
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
            MTS_CHECK(glfwInit() == GLFW_TRUE, "glfwInit failed");
        }

        // No OpenGL context. The renderer owns the graphics API.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, desc.m_resizable ? GLFW_TRUE : GLFW_FALSE);

        m_handle = glfwCreateWindow(static_cast<int>(desc.m_width),
                                    static_cast<int>(desc.m_height),
                                    desc.m_title,
                                    nullptr,
                                    nullptr);
        MTS_CHECK(m_handle != nullptr, "glfwCreateWindow failed");
        ++g_windowCount;

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(m_handle, &fbWidth, &fbHeight);
        m_width = static_cast<uint32_t>(fbWidth);
        m_height = static_cast<uint32_t>(fbHeight);

        // Lets the static callbacks recover the owning instance.
        glfwSetWindowUserPointer(m_handle, this);
        glfwSetFramebufferSizeCallback(m_handle, &GLFWWindow::OnFramebufferSize);

        MTS_LOG_INFO("Window created: {}x{} \"{}\"", m_width, m_height, desc.m_title);
    }

    GLFWWindow::~GLFWWindow()
    {
        if (m_handle != nullptr)
        {
            glfwDestroyWindow(m_handle);
            m_handle = nullptr;
            --g_windowCount;
        }

        if (g_windowCount == 0)
        {
            glfwTerminate();
        }
    }

    void GLFWWindow::PollEvents()
    {
        glfwPollEvents();
    }

    bool GLFWWindow::ShouldClose() const
    {
        return glfwWindowShouldClose(m_handle) == GLFW_TRUE;
    }

    void GLFWWindow::OnFramebufferSize(GLFWwindow *handle, int width, int height)
    {
        auto *self = static_cast<GLFWWindow *>(glfwGetWindowUserPointer(handle));
        MTS_ASSERT(self != nullptr, "framebuffer callback without a user pointer");

        // Minimizing reports 0x0. Keep it; the renderer decides to skip frames.
        self->m_width = static_cast<uint32_t>(width);
        self->m_height = static_cast<uint32_t>(height);
    }

    void *GLFWWindow::NativeWindow() const
    {
#if ENGINE_PLATFORM_WINDOWS
        return glfwGetWin32Window(m_handle);
#elif ENGINE_PLATFORM_MACOS
        return glfwGetCocoaWindow(m_handle);
#elif ENGINE_PLATFORM_LINUX
        return reinterpret_cast<void *>(glfwGetX11Window(m_handle));
#else
        return nullptr;
#endif
    }
}
