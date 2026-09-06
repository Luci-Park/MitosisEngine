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

#include "core/log/Assert.h"

namespace mts
{
    // Never actually created (Show() always returns false below), but
    // unique_ptr<SplashImpl>'s destructor still needs the type complete
    // wherever it's instantiated - including here.
    struct SplashImpl
    {
    };

    SplashScreen::SplashScreen() = default;
    SplashScreen::~SplashScreen() = default;

    bool SplashScreen::Show(const SplashDesc &) { return false; }

    void SplashScreen::SetProgress(const char *status, float)
    {
        MTS_ASSERT(status != nullptr, "SplashScreen::SetProgress: status must not be null");
    }

    void SplashScreen::Close() {}
}
