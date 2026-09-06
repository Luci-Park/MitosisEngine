/**
 * @file SplashLinux.cpp
 * @author Rahul Nair
 * @brief No splash backend on Linux yet - Show() is a no-op so App::Initialize
 * doesn't need to care whether a splash actually appeared.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include "window/Splash.h"

namespace mts
{
    SplashScreen::~SplashScreen() = default;

    bool SplashScreen::Show(const SplashDesc &) { return false; }

    void SplashScreen::SetProgress(const char *, float) {}

    void SplashScreen::Close() {}
}
