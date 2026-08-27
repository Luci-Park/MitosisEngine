/**
 * @file GLFWWindow.h
 * @author Sumin Park
 * @brief GLFW window for desktop platforms
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include "window/Window.h"

struct GLFWwindow;

namespace mts
{
    class GLFWWindow final : public Window
    {
    public:
        explicit GLFWWindow(const WindowDesc &desc);
        ~GLFWWindow() override;

        void PollEvents() override;
        bool ShouldClose() const override;

        uint32_t Width() const override { return m_width; }
        uint32_t Height() const override { return m_height; }

        void *NativeWindow() const override;

    private:
        static void OnFramebufferSize(GLFWwindow *handle, int width, int height);

        GLFWwindow *m_handle = nullptr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
    };
}
