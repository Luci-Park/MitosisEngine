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

        uint32_t Width() const override { return mWidth; }
        uint32_t Height() const override { return mHeight; }

        NativeWindowHandle NativeWindow() const override;
        void *NativeHandleForImGui() const override { return mHandle; }

    private:
        static void OnFramebufferSize(GLFWwindow *handle, int width, int height);

        GLFWwindow *mHandle = nullptr;
        uint32_t mWidth = 0;
        uint32_t mHeight = 0;
    };
}
