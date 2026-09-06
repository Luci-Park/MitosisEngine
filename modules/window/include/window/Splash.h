/**
 * @file Splash.h
 * @author Rahul Nair
 * @brief Startup splash screen shown before Window/VulkanRenderer exist.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#pragma once

namespace mts
{
    struct SplashDesc
    {
        const char *mEngineName = "MitosisEngine";
        const char *mVersion = "";
        const char *mCopyright = "";
        const char *mStatus = "Loading...";

        float mProgress = 0.0f;
    };

    class SplashScreen
    {
    public:
        SplashScreen() = default;
        ~SplashScreen();

        SplashScreen(const SplashScreen &) = delete;
        SplashScreen &operator=(const SplashScreen &) = delete;
        SplashScreen(SplashScreen &&) = delete;
        SplashScreen &operator=(SplashScreen &&) = delete;

        static constexpr int kWidth = 640;
        static constexpr int kHeight = 360;

        bool Show(const SplashDesc &desc);

        void SetProgress(const char *status, float progress);

        void Close();

        bool IsShowing() const { return mHandle != nullptr; }

    private:
        void *mHandle = nullptr;
    };
}
