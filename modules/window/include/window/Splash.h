/**
 * @file Splash.h
 * @author Rahul Nair
 * @brief Startup splash screen shown before Window/VulkanRenderer exist.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

#include <memory>

namespace mir
{
    struct SplashImpl;

    struct SplashDesc
    {
        const char *mEngineName = "MjolnirEngine";
        const char *mVersion = "";
        const char *mCopyright = "";
        const char *mStatus = "Loading...";

        float mProgress = 0.0f;
    };

    class SplashScreen
    {
    public:
        SplashScreen();
        ~SplashScreen();

        SplashScreen(const SplashScreen &) = delete;
        SplashScreen &operator=(const SplashScreen &) = delete;
        SplashScreen(SplashScreen &&) = delete;
        SplashScreen &operator=(SplashScreen &&) = delete;

        static constexpr int kWidth = 1280;
        static constexpr int kHeight = 720;

        bool Show(const SplashDesc &desc);

        void SetProgress(const char *status, float progress);

        void Close();

        bool IsShowing() const { return mImpl != nullptr; }

    private:
        std::unique_ptr<SplashImpl> mImpl;
    };
}
