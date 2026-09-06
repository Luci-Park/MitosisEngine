/**
 * @file SplashWin32.cpp
 * @author Rahul Nair
 * @brief Win32 GDI implementation of the startup splash screen.
 *
 * @copyright Copyright (c) 2026 DigiPen (USA) Corporation
 *
 */
#include "window/Splash.h"

#include "core/fs/Paths.h"
#include "core/log/Assert.h"
#include "core/log/Log.h"

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

#include <wincodec.h>

#include <algorithm>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
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
    }

    struct SplashImpl
    {
        std::thread mThread;
        HWND mWindow = nullptr;
        std::mutex mMutex;
        PaintState mState;
    };

    namespace
    {
        std::wstring Utf8ToWide(const std::string &utf8)
        {
            if (utf8.empty())
                return {};

            const int len = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            std::wstring wide(static_cast<size_t>(len), L'\0');
            ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), len);
            return wide;
        }

        struct ComGuard
        {
            HRESULT hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            ~ComGuard()
            {
                if (SUCCEEDED(hr))
                    ::CoUninitialize();
            }
        };

        HBITMAP LoadArtworkBitmap(const std::filesystem::path &path, int targetWidth, int targetHeight)
        {
            IWICImagingFactory *factory = nullptr;
            if (FAILED(::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&factory))))
                return nullptr;

            IWICBitmapDecoder *decoder = nullptr;
            const HRESULT decodeHr = factory->CreateDecoderFromFilename(
                path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
            if (FAILED(decodeHr))
            {
                factory->Release();
                return nullptr;
            }

            IWICBitmapFrameDecode *frame = nullptr;
            const HRESULT frameHr = decoder->GetFrame(0, &frame);
            decoder->Release();
            if (FAILED(frameHr))
            {
                factory->Release();
                return nullptr;
            }

            IWICBitmapScaler *scaler = nullptr;
            HRESULT hr = factory->CreateBitmapScaler(&scaler);
            if (SUCCEEDED(hr))
                hr = scaler->Initialize(frame, targetWidth, targetHeight, WICBitmapInterpolationModeFant);
            frame->Release();
            if (FAILED(hr))
            {
                if (scaler != nullptr)
                    scaler->Release();
                factory->Release();
                return nullptr;
            }

            IWICFormatConverter *converter = nullptr;
            hr = factory->CreateFormatConverter(&converter);
            if (SUCCEEDED(hr))
            {
                hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppBGR,
                                            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
            }
            scaler->Release();
            factory->Release();
            if (FAILED(hr))
            {
                if (converter != nullptr)
                    converter->Release();
                return nullptr;
            }

            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = targetWidth;
            bmi.bmiHeader.biHeight = -targetHeight; // top-down, matches WIC's row order
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            void *bits = nullptr;
            HBITMAP bitmap = ::CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (bitmap == nullptr || bits == nullptr)
            {
                converter->Release();
                if (bitmap != nullptr)
                    ::DeleteObject(bitmap);
                return nullptr;
            }

            const UINT stride = static_cast<UINT>(targetWidth) * 4;
            const HRESULT copyHr = converter->CopyPixels(
                nullptr, stride, stride * static_cast<UINT>(targetHeight), static_cast<BYTE *>(bits));
            converter->Release();

            if (FAILED(copyHr))
            {
                ::DeleteObject(bitmap);
                return nullptr;
            }

            return bitmap;
        }

        void DrawSplash(HWND hwnd, HDC dc, const PaintState &state)
        {
            RECT client{};
            ::GetClientRect(hwnd, &client);

            static HBRUSH artBrush = ::CreateSolidBrush(RGB(30, 30, 36));
            static HBRUSH stripBrush = ::CreateSolidBrush(RGB(18, 18, 22));
            static HBRUSH trackBrush = ::CreateSolidBrush(RGB(45, 45, 52));
            static HBRUSH fillBrush = ::CreateSolidBrush(RGB(90, 140, 235));
            static HFONT nameFont = ::CreateFontW(
                36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            static HFONT smallFont = ::CreateFontW(
                14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            static HBITMAP artBitmap = [] {
                HBITMAP bmp = LoadArtworkBitmap(BrandingPath("wanderer-splash-screen.jpg"),
                                                 SplashScreen::kWidth, SplashScreen::kHeight - kStripHeight);
                if (bmp == nullptr)
                    MTS_LOG_WARN("Splash: could not load branding artwork, using placeholder fill");
                return bmp;
            }();

            RECT artRect{0, 0, client.right, client.bottom - kStripHeight};
            if (artBitmap != nullptr)
            {
                HDC memDc = ::CreateCompatibleDC(dc);
                HGDIOBJ prevBitmap = ::SelectObject(memDc, artBitmap);
                ::BitBlt(dc, artRect.left, artRect.top, artRect.right - artRect.left,
                         artRect.bottom - artRect.top, memDc, 0, 0, SRCCOPY);
                ::SelectObject(memDc, prevBitmap);
                ::DeleteDC(memDc);
            }
            else
            {
                ::FillRect(dc, &artRect, artBrush);
            }

            RECT stripRect{0, artRect.bottom, client.right, client.bottom};
            ::FillRect(dc, &stripRect, stripBrush);

            ::SetBkMode(dc, TRANSPARENT);

            // Engine name, centered in the artwork placeholder.
            HGDIOBJ prevFont = ::SelectObject(dc, nameFont);
            ::SetTextColor(dc, RGB(230, 230, 235));
            RECT nameRect = artRect;
            const std::wstring engineName = Utf8ToWide(state.mEngineName);
            ::DrawTextW(dc, engineName.c_str(), -1, &nameRect,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Status text, top-left of the bottom strip.
            ::SelectObject(dc, smallFont);
            ::SetTextColor(dc, RGB(200, 200, 205));
            RECT statusRect{kMargin, stripRect.top + 14, client.right - kMargin, stripRect.top + 34};
            const std::wstring status = Utf8ToWide(state.mStatus);
            ::DrawTextW(dc, status.c_str(), -1, &statusRect,
                        DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

            // Progress bar under the status text.
            RECT barTrack{kMargin, statusRect.bottom + 6, client.right - kMargin, statusRect.bottom + 6 + kBarHeight};
            ::FillRect(dc, &barTrack, trackBrush);

            const float progress = std::clamp(state.mProgress, 0.0f, 1.0f);
            RECT barFill = barTrack;
            barFill.right = barTrack.left + static_cast<LONG>((barTrack.right - barTrack.left) * progress);
            if (barFill.right > barFill.left)
            {
                ::FillRect(dc, &barFill, fillBrush);
            }

            // Version (bottom-left) + copyright (bottom-right).
            RECT versionRect{kMargin, client.bottom - 26, client.right / 2, client.bottom - 6};
            const std::wstring version = Utf8ToWide(state.mVersion);
            ::DrawTextW(dc, version.c_str(), -1, &versionRect, DT_LEFT | DT_SINGLELINE);

            RECT copyrightRect{client.right / 2, client.bottom - 26, client.right - kMargin, client.bottom - 6};
            const std::wstring copyright = Utf8ToWide(state.mCopyright);
            ::DrawTextW(dc, copyright.c_str(), -1, &copyrightRect, DT_RIGHT | DT_SINGLELINE);

            ::SelectObject(dc, prevFont);
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
                wc.hbrBackground = ::CreateSolidBrush(RGB(18, 18, 22));
                ::RegisterClassExW(&wc);
            });
        }

        void SplashThreadMain(SplashImpl *impl, SplashDesc desc, std::promise<HWND> ready)
        {
            ComGuard comGuard;

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

            PaintState initialState;
            try
            {
                initialState = PaintState{desc.mEngineName, desc.mVersion, desc.mCopyright, desc.mStatus, desc.mProgress};
                std::lock_guard<std::mutex> lock(impl->mMutex);
                impl->mState = initialState;
            }
            catch (...)
            {
                ::DestroyWindow(hwnd);
                ready.set_value(nullptr);
                return;
            }

            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));

            if (HDC dc = ::GetDC(hwnd))
            {
                DrawSplash(hwnd, dc, initialState);
                ::ReleaseDC(hwnd, dc);
            }

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

    SplashScreen::SplashScreen() = default;

    SplashScreen::~SplashScreen()
    {
        Close();
    }

    bool SplashScreen::Show(const SplashDesc &desc)
    {
        Close();

        auto impl = std::make_unique<SplashImpl>();

        std::promise<HWND> ready;
        std::future<HWND> readyFuture = ready.get_future();

        try
        {
            // The thread only needs to reach *impl, not own it - impl stays
            // with this function (then mImpl) the whole time.
            impl->mThread = std::thread(&SplashThreadMain, impl.get(), desc, std::move(ready));
        }
        catch (const std::system_error &)
        {
            // Thread never started - nothing to join.
            return false;
        }

        HWND hwnd = readyFuture.get();
        if (hwnd == nullptr)
        {
            impl->mThread.join();
            return false;
        }

        impl->mWindow = hwnd;
        mImpl = std::move(impl);
        return true;
    }

    void SplashScreen::SetProgress(const char *status, float progress)
    {
        MTS_ASSERT(status != nullptr, "SplashScreen::SetProgress: status must not be null");

        if (mImpl == nullptr)
            return;

        {
            std::lock_guard<std::mutex> lock(mImpl->mMutex);
            mImpl->mState.mStatus = status;
            mImpl->mState.mProgress = progress;
        }

        ::PostMessageW(mImpl->mWindow, kUpdateMessage, 0, 0);
    }

    void SplashScreen::Close()
    {
        if (mImpl == nullptr)
            return;

        ::PostMessageW(mImpl->mWindow, WM_CLOSE, 0, 0);
        mImpl->mThread.join();
        mImpl.reset();
    }
}
