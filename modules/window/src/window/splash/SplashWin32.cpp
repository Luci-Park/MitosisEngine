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
#include <string>

namespace mts
{
    namespace
    {
        constexpr wchar_t kClassName[] = L"MtsSplashWindow";

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

        void DrawSplash(HWND hwnd, HDC dc, const PaintState &state)
        {
            RECT client{};
            ::GetClientRect(hwnd, &client);

            // Artwork placeholder: solid fill until real art is dropped in -
            // swapping it for an image blit later does not touch layout.
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
                auto *state = reinterpret_cast<PaintState *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                PAINTSTRUCT ps{};
                HDC dc = ::BeginPaint(hwnd, &ps);
                if (state != nullptr)
                    DrawSplash(hwnd, dc, *state);
                ::EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_ERASEBKGND:
                // DrawSplash repaints the whole client area every time -
                // skip the default erase so there is no white flash first.
                return 1;
            default:
                return ::DefWindowProcW(hwnd, msg, wParam, lParam);
            }
        }

        void RegisterSplashClassOnce()
        {
            static bool registered = false;
            if (registered)
                return;

            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = &SplashWndProc;
            wc.hInstance = ::GetModuleHandleW(nullptr);
            wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
            wc.lpszClassName = kClassName;
            ::RegisterClassExW(&wc);
            registered = true;
        }
    }

    SplashScreen::~SplashScreen()
    {
        Close();
    }

    bool SplashScreen::Show(const SplashDesc &desc)
    {
        Close();

        RegisterSplashClassOnce();

        const int screenW = ::GetSystemMetrics(SM_CXSCREEN);
        const int screenH = ::GetSystemMetrics(SM_CYSCREEN);
        const int x = (screenW - kWidth) / 2;
        const int y = (screenH - kHeight) / 2;

        HWND hwnd = ::CreateWindowExW(
            WS_EX_TOPMOST, kClassName, L"", WS_POPUP,
            x, y, kWidth, kHeight,
            nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
        if (hwnd == nullptr)
            return false;

        auto *state = new PaintState{desc.mEngineName, desc.mVersion, desc.mCopyright, desc.mStatus, desc.mProgress};
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        ::ShowWindow(hwnd, SW_SHOW);
        // Forces WM_PAINT synchronously, so the splash is actually on screen
        // before Initialize goes on to block the thread with real work.
        ::UpdateWindow(hwnd);

        mHandle = hwnd;
        return true;
    }

    void SplashScreen::Close()
    {
        if (mHandle == nullptr)
            return;

        HWND hwnd = static_cast<HWND>(mHandle);
        auto *state = reinterpret_cast<PaintState *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        delete state;
        ::DestroyWindow(hwnd);
        mHandle = nullptr;
    }
}
