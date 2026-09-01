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
        glfwWindowHint(GLFW_RESIZABLE, desc.mResizable ? GLFW_TRUE : GLFW_FALSE);

        mHandle = glfwCreateWindow(static_cast<int>(desc.mWidth),
                                    static_cast<int>(desc.mHeight),
                                    desc.mTitle,
                                    nullptr,
                                    nullptr);
        MTS_CHECK(mHandle != nullptr, "glfwCreateWindow failed");
        ++g_windowCount;

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(mHandle, &fbWidth, &fbHeight);
        mWidth = static_cast<uint32_t>(fbWidth);
        mHeight = static_cast<uint32_t>(fbHeight);

        // Lets the static callbacks recover the owning instance.
        glfwSetWindowUserPointer(mHandle, this);
        glfwSetFramebufferSizeCallback(mHandle, &GLFWWindow::OnFramebufferSize);

        MTS_LOG_INFO("Window created: {}x{} \"{}\"", mWidth, mHeight, desc.mTitle);
    }

    GLFWWindow::~GLFWWindow()
    {
        if (mHandle != nullptr)
        {
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
        glfwPollEvents();
    }

    bool GLFWWindow::ShouldClose() const
    {
        return glfwWindowShouldClose(mHandle) == GLFW_TRUE;
    }

    void GLFWWindow::OnFramebufferSize(GLFWwindow *handle, int width, int height)
    {
        auto *self = static_cast<GLFWWindow *>(glfwGetWindowUserPointer(handle));
        MTS_ASSERT(self != nullptr, "framebuffer callback without a user pointer");

        // Minimizing reports 0x0. Keep it; the renderer decides to skip frames.
        self->mWidth = static_cast<uint32_t>(width);
        self->mHeight = static_cast<uint32_t>(height);
    }
}
