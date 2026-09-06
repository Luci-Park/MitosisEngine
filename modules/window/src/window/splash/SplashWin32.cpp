/**
 * @file SplashWin32.cpp
 * @author Rahul Nair
 * @brief Win32 GDI implementation of the startup splash screen.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include "window/Splash.h"

// This file uses the -W Win32 APIs throughout (CreateWindowExW,
// RegisterClassExW, ...); without UNICODE, resource macros like IDC_ARROW
// resolve to their -A (LPSTR) form and fail to convert.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>

#include <algorithm>
#include <future>
#include <mutex>
#include <string>
#include <thread>

namespace mts
{
    namespace
    {
        constexpr wchar_t kClassName[] = L"MtsSplashWindow";

        constexpr UINT kUpdateMessage = WM_APP + 1;

        // Bottom strip: status text + progress bar + version/copyright.
        constexpr int kStripHeight = 100;
        constexpr int kMargin = 20;
        constexpr int kBarHeight = 6;

        // Owns copies of the desc strings for the window's lifetime - the
        // caller's SplashDesc (often built from temporaries) does not need
        // to outlive Show().
        struct PaintState
        {
            std::string mEngineName;
            std::string mVersion;
            std::string mCopyright;
            std::string mStatus;
            float mProgress = 0.0f;
        };

        struct SplashImpl
        {
            std::thread mThread;
            HWND mWindow = nullptr;
            std::mutex mMutex;
            PaintState mState;
        };

        void DrawSplash(HWND hwnd, HDC dc, const PaintState &state)
        {
            RECT client{};
            ::GetClientRect(hwnd, &client);

            RECT artRect{0, 0, client.right, client.bottom - kStripHeight};
            HBRUSH artBrush = ::CreateSolidBrush(RGB(30, 30, 36));
            ::FillRect(dc, &artRect, artBrush);
            ::DeleteObject(artBrush);

            RECT stripRect{0, artRect.bottom, client.right, client.bottom};
            HBRUSH stripBrush = ::CreateSolidBrush(RGB(18, 18, 22));
            ::FillRect(dc, &stripRect, stripBrush);
            ::DeleteObject(stripBrush);

            ::SetBkMode(dc, TRANSPARENT);

            HFONT nameFont = ::CreateFontA(
                36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
            HFONT smallFont = ::CreateFontA(
                14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");

            // Engine name, centered in the artwork placeholder.
            HGDIOBJ prevFont = ::SelectObject(dc, nameFont);
            ::SetTextColor(dc, RGB(230, 230, 235));
            RECT nameRect = artRect;
            ::DrawTextA(dc, state.mEngineName.c_str(), -1, &nameRect,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Status text, top-left of the bottom strip.
            ::SelectObject(dc, smallFont);
            ::SetTextColor(dc, RGB(200, 200, 205));
            RECT statusRect{kMargin, stripRect.top + 14, client.right - kMargin, stripRect.top + 34};
            ::DrawTextA(dc, state.mStatus.c_str(), -1, &statusRect,
                        DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Progress bar under the status text.
            RECT barTrack{kMargin, statusRect.bottom + 6, client.right - kMargin, statusRect.bottom + 6 + kBarHeight};
            HBRUSH trackBrush = ::CreateSolidBrush(RGB(45, 45, 52));
            ::FillRect(dc, &barTrack, trackBrush);
            ::DeleteObject(trackBrush);

            const float progress = std::clamp(state.mProgress, 0.0f, 1.0f);
            RECT barFill = barTrack;
            barFill.right = barTrack.left + static_cast<LONG>((barTrack.right - barTrack.left) * progress);
            if (barFill.right > barFill.left)
            {
                HBRUSH fillBrush = ::CreateSolidBrush(RGB(90, 140, 235));
                ::FillRect(dc, &barFill, fillBrush);
                ::DeleteObject(fillBrush);
            }

            // Version (bottom-left) + copyright (bottom-right).
            RECT versionRect{kMargin, client.bottom - 26, client.right / 2, client.bottom - 6};
            ::DrawTextA(dc, state.mVersion.c_str(), -1, &versionRect, DT_LEFT | DT_SINGLELINE);

            RECT copyrightRect{client.right / 2, client.bottom - 26, client.right - kMargin, client.bottom - 6};
            ::DrawTextA(dc, state.mCopyright.c_str(), -1, &copyrightRect, DT_RIGHT | DT_SINGLELINE);

            ::SelectObject(dc, prevFont);
            ::DeleteObject(nameFont);
            ::DeleteObject(smallFont);
        }

        LRESULT CALLBACK SplashWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            switch (msg)
            {
            case WM_PAINT:
            {
                auto *impl = reinterpret_cast<SplashImpl *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                PAINTSTRUCT ps{};
                HDC dc = ::BeginPaint(hwnd, &ps);
                if (impl != nullptr)
                {
                    PaintState snapshot;
                    {
                        std::lock_guard<std::mutex> lock(impl->mMutex);
                        snapshot = impl->mState;
                    }
                    DrawSplash(hwnd, dc, snapshot);
                }
                ::EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_ERASEBKGND:
                // DrawSplash repaints the whole client area every time -
                // skip the default erase so there is no white flash first.
                return 1;
            case kUpdateMessage:
                ::InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            case WM_DESTROY:
                // Ends this thread's GetMessageW loop in Close().
                ::PostQuitMessage(0);
                return 0;
            default:
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);
            }
        }

        void RegisterSplashClassOnce()
        {
            static std::once_flag once;
            std::call_once(once, [] {
                WNDCLASSEXW wc{};
                wc.cbSize = sizeof(wc);
                wc.lpfnWndProc = &SplashWndProc;
                wc.hInstance = ::GetModuleHandleW(nullptr);
                wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
                wc.lpszClassName = kClassName;
                ::RegisterClassExW(&wc);
            });
        }

        void SplashThreadMain(SplashImpl *impl, SplashDesc desc, std::promise<HWND> ready)
        {
            RegisterSplashClassOnce();

            const int screenW = ::GetSystemMetrics(SM_CXSCREEN);
            const int screenH = ::GetSystemMetrics(SM_CYSCREEN);
            const int x = (screenW - SplashScreen::kWidth) / 2;
            const int y = (screenH - SplashScreen::kHeight) / 2;

            HWND hwnd = ::CreateWindowExW(
                WS_EX_TOPMOST, kClassName, L"", WS_POPUP,
                x, y, SplashScreen::kWidth, SplashScreen::kHeight,
                nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
            if (hwnd == nullptr)
            {
                ready.set_value(nullptr);
                return;
            }

            {
                std::lock_guard<std::mutex> lock(impl->mMutex);
                impl->mState = PaintState{desc.mEngineName, desc.mVersion, desc.mCopyright, desc.mStatus, desc.mProgress};
            }

            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
            ::ShowWindow(hwnd, SW_SHOW);
            ::UpdateWindow(hwnd);

            ready.set_value(hwnd);

            MSG msg{};
            while (::GetMessageW(&msg, nullptr, 0, 0) > 0)
            {
                ::TranslateMessage(&msg);
                ::DispatchMessageW(&msg);
            }
        }
    }

    SplashScreen::~SplashScreen()
    {
        Close();
    }

    bool SplashScreen::Show(const SplashDesc &desc)
    {
        Close();

        auto *impl = new SplashImpl();

        std::promise<HWND> ready;
        std::future<HWND> readyFuture = ready.get_future();
        impl->mThread = std::thread(&SplashThreadMain, impl, desc, std::move(ready));

        HWND hwnd = readyFuture.get();
        if (hwnd == nullptr)
        {
            impl->mThread.join();
            delete impl;
            return false;
        }

        impl->mWindow = hwnd;
        mImpl = impl;
        return true;
    }

    void SplashScreen::SetProgress(const char *status, float progress)
    {
        if (mImpl == nullptr)
            return;

        auto *impl = static_cast<SplashImpl *>(mImpl);
        {
            std::lock_guard<std::mutex> lock(impl->mMutex);
            impl->mState.mStatus = status;
            impl->mState.mProgress = progress;
        }

        ::PostMessageW(impl->mWindow, kUpdateMessage, 0, 0);
    }

    void SplashScreen::Close()
    {
        if (mImpl == nullptr)
            return;

        auto *impl = static_cast<SplashImpl *>(mImpl);
        ::PostMessageW(impl->mWindow, WM_CLOSE, 0, 0);
        impl->mThread.join();
        delete impl;
        mImpl = nullptr;
    }
}
