#pragma once

#include "common.h"

namespace ModernScreenshot {

template <typename T>
class ScopedGdiObject {
public:
    ScopedGdiObject() : handle_(NULL) {}
    explicit ScopedGdiObject(T handle) : handle_(handle) {}
    ~ScopedGdiObject() { reset(); }

    ScopedGdiObject(const ScopedGdiObject &) = delete;
    ScopedGdiObject &operator=(const ScopedGdiObject &) = delete;

    ScopedGdiObject(ScopedGdiObject &&other) noexcept : handle_(other.handle_) {
        other.handle_ = NULL;
    }

    ScopedGdiObject &operator=(ScopedGdiObject &&other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            other.handle_ = NULL;
        }
        return *this;
    }

    T get() const { return handle_; }
    operator T() const { return handle_; }
    explicit operator bool() const { return handle_ != NULL; }

    T release() {
        T temp = handle_;
        handle_ = NULL;
        return temp;
    }

    void reset(T newHandle = NULL) {
        if (handle_) {
            DeleteObject(handle_);
        }
        handle_ = newHandle;
    }

private:
    T handle_;
};

using ScopedBitmap = ScopedGdiObject<HBITMAP>;
using ScopedBrush = ScopedGdiObject<HBRUSH>;
using ScopedFont = ScopedGdiObject<HFONT>;
using ScopedPen = ScopedGdiObject<HPEN>;

class ScopedDC {
public:
    ScopedDC() : hdc_(NULL), hwnd_(NULL), isWindowDC_(false) {}

    // Compatible DC (created via CreateCompatibleDC)
    static ScopedDC Compatible(HDC base) {
        ScopedDC s;
        s.hdc_ = CreateCompatibleDC(base);
        s.isWindowDC_ = false;
        return s;
    }

    // Window DC (obtained via GetDC)
    static ScopedDC FromWindow(HWND hwnd) {
        ScopedDC s;
        s.hwnd_ = hwnd;
        s.hdc_ = GetDC(hwnd);
        s.isWindowDC_ = true;
        return s;
    }

    ~ScopedDC() { reset(); }

    ScopedDC(const ScopedDC &) = delete;
    ScopedDC &operator=(const ScopedDC &) = delete;

    ScopedDC(ScopedDC &&other) noexcept
        : hdc_(other.hdc_), hwnd_(other.hwnd_), isWindowDC_(other.isWindowDC_) {
        other.hdc_ = NULL;
        other.hwnd_ = NULL;
    }

    ScopedDC &operator=(ScopedDC &&other) noexcept {
        if (this != &other) {
            reset();
            hdc_ = other.hdc_;
            hwnd_ = other.hwnd_;
            isWindowDC_ = other.isWindowDC_;
            other.hdc_ = NULL;
            other.hwnd_ = NULL;
        }
        return *this;
    }

    HDC get() const { return hdc_; }
    operator HDC() const { return hdc_; }
    explicit operator bool() const { return hdc_ != NULL; }

    void reset() {
        if (hdc_) {
            if (isWindowDC_) {
                ReleaseDC(hwnd_, hdc_);
            } else {
                DeleteDC(hdc_);
            }
            hdc_ = NULL;
            hwnd_ = NULL;
        }
    }

private:
    HDC hdc_;
    HWND hwnd_;
    bool isWindowDC_;
};

class ScopedSelectObject {
public:
    ScopedSelectObject(HDC dc, HGDIOBJ newObj)
        : dc_(dc), oldObj_(NULL) {
        if (dc_ && newObj) {
            oldObj_ = SelectObject(dc_, newObj);
        }
    }

    ~ScopedSelectObject() {
        if (dc_ && oldObj_) {
            SelectObject(dc_, oldObj_);
        }
    }

    ScopedSelectObject(const ScopedSelectObject &) = delete;
    ScopedSelectObject &operator=(const ScopedSelectObject &) = delete;

private:
    HDC dc_;
    HGDIOBJ oldObj_;
};

class GdiplusManager {
public:
    static bool EnsureStarted() {
        if (token_) return true;
        Gdiplus::GdiplusStartupInput input;
        return Gdiplus::GdiplusStartup(&token_, &input, NULL) == Gdiplus::Ok;
    }

    static void Shutdown() {
        if (token_) {
            Gdiplus::GdiplusShutdown(token_);
            token_ = 0;
        }
    }

private:
    static inline ULONG_PTR token_ = 0;
};

} // namespace ModernScreenshot
