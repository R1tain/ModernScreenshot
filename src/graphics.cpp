#include "graphics.h"

namespace ModernScreenshot {

static ULONG_PTR g_gdiplusToken = 0;

bool StartGdiplus() {
    if (g_gdiplusToken) {
        return true;
    }
    Gdiplus::GdiplusStartupInput input;
    return Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, NULL) == Gdiplus::Ok;
}

void StopGdiplus() {
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

bool GetEncoderClsid(const wchar_t *mimeType, CLSID *clsid) {
    UINT count = 0;
    UINT bytes = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &bytes) != Gdiplus::Ok || bytes == 0) {
        return false;
    }

    std::vector<BYTE> storage(bytes);
    ImageCodecInfo *codecs = reinterpret_cast<ImageCodecInfo *>(storage.data());
    if (Gdiplus::GetImageEncoders(count, bytes, codecs) != Gdiplus::Ok) {
        return false;
    }

    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(codecs[i].MimeType, mimeType) == 0) {
            *clsid = codecs[i].Clsid;
            return true;
        }
    }
    return false;
}

bool SaveHbitmapPng(HBITMAP bitmap, const std::wstring &path) {
    if (!StartGdiplus()) {
        return false;
    }

    CLSID pngClsid;
    if (!GetEncoderClsid(L"image/png", &pngClsid)) {
        return false;
    }

    Bitmap image(bitmap, NULL);
    return image.Save(path.c_str(), &pngClsid, NULL) == Gdiplus::Ok;
}

ScreenCapture CaptureVirtualScreen() {
    ScreenCapture capture;
    capture.x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    capture.y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    capture.w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    capture.h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC screen = GetDC(NULL);
    HDC memory = CreateCompatibleDC(screen);
    capture.bitmap = CreateCompatibleBitmap(screen, capture.w, capture.h);
    HGDIOBJ old = SelectObject(memory, capture.bitmap);
    BitBlt(memory, 0, 0, capture.w, capture.h, screen, capture.x, capture.y, SRCCOPY | CAPTUREBLT);
    SelectObject(memory, old);
    DeleteDC(memory);
    ReleaseDC(NULL, screen);
    return capture;
}

HBITMAP CropCapture(const ScreenCapture &capture, const RECT &selection) {
    int w = RectWidth(selection);
    int h = RectHeight(selection);
    if (w <= 0 || h <= 0) {
        return NULL;
    }

    HDC screen = GetDC(NULL);
    HDC source = CreateCompatibleDC(screen);
    HDC dest = CreateCompatibleDC(screen);
    HBITMAP cropped = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ oldSource = SelectObject(source, capture.bitmap);
    HGDIOBJ oldDest = SelectObject(dest, cropped);
    BitBlt(dest, 0, 0, w, h, source, selection.left - capture.x, selection.top - capture.y, SRCCOPY);
    SelectObject(source, oldSource);
    SelectObject(dest, oldDest);
    DeleteDC(source);
    DeleteDC(dest);
    ReleaseDC(NULL, screen);
    return cropped;
}

HBITMAP CaptureScreenRect(const RECT &rect) {
    int w = RectWidth(rect);
    int h = RectHeight(rect);
    if (w <= 0 || h <= 0) {
        return NULL;
    }

    HDC screen = GetDC(NULL);
    HDC dest = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, w, h);
    if (!dest || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (dest) DeleteDC(dest);
        ReleaseDC(NULL, screen);
        return NULL;
    }

    HGDIOBJ oldDest = SelectObject(dest, bitmap);
    BitBlt(dest, 0, 0, w, h, screen, rect.left, rect.top, SRCCOPY | CAPTUREBLT);
    SelectObject(dest, oldDest);
    DeleteDC(dest);
    ReleaseDC(NULL, screen);
    return bitmap;
}

HBITMAP CopyBitmapStrip(HBITMAP sourceBitmap, int w, int sourceY, int copyH) {
    if (!sourceBitmap || w <= 0 || copyH <= 0) {
        return NULL;
    }

    HDC screen = GetDC(NULL);
    HDC source = CreateCompatibleDC(screen);
    HDC dest = CreateCompatibleDC(screen);
    HBITMAP strip = CreateCompatibleBitmap(screen, w, copyH);
    if (!source || !dest || !strip) {
        if (strip) DeleteObject(strip);
        if (source) DeleteDC(source);
        if (dest) DeleteDC(dest);
        ReleaseDC(NULL, screen);
        return NULL;
    }

    HGDIOBJ oldSource = SelectObject(source, sourceBitmap);
    HGDIOBJ oldDest = SelectObject(dest, strip);
    BitBlt(dest, 0, 0, w, copyH, source, 0, sourceY, SRCCOPY);
    SelectObject(source, oldSource);
    SelectObject(dest, oldDest);
    DeleteDC(source);
    DeleteDC(dest);
    ReleaseDC(NULL, screen);
    return strip;
}

bool ReadBitmapPixels(HBITMAP bitmap, int w, int h, std::vector<BYTE> *pixels) {
    if (!bitmap || w <= 0 || h <= 0) {
        return false;
    }

    pixels->assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0);
    HDC screen = GetDC(NULL);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    int lines = GetDIBits(screen, bitmap, 0, h, pixels->data(), &info, DIB_RGB_COLORS);
    ReleaseDC(NULL, screen);
    return lines == h;
}

void FillRectColor(HDC dc, const RECT &rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void DrawTextLabel(HDC dc, const std::wstring &text, int x, int y, COLORREF color) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HFONT font = CreateFontW(-14, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void DrawCenteredText(HDC dc, const wchar_t *text, const RECT &rect, int fontHeight, int weight, COLORREF color) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HFONT font = CreateFontW(fontHeight, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE size;
    int len = static_cast<int>(wcslen(text));
    GetTextExtentPoint32W(dc, text, len, &size);
    TextOutW(dc, rect.left + (RectWidth(rect) - size.cx) / 2,
             rect.top + (RectHeight(rect) - size.cy) / 2,
             text, len);
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

void ApplyWindowGlassStyle(HWND hwnd, int cornerRadius, BYTE alpha) {
    if (!hwnd || !IsWindow(hwnd)) return;

    // Apply rounded window region for crisp corners on all Windows versions
    if (cornerRadius > 0) {
        RECT r = {};
        if (GetWindowRect(hwnd, &r)) {
            int w = RectWidth(r);
            int h = RectHeight(r);
            if (w > 0 && h > 0) {
                HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, cornerRadius * 2, cornerRadius * 2);
                SetWindowRgn(hwnd, rgn, TRUE);
            }
        }
    }

    // Enable layered window alpha for subtle Apple macOS vibrancy translucency
    if (alpha < 255) {
        LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
    }

    // Enable DWM dark mode & round corners on Windows 11 if available
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (dwm) {
        typedef HRESULT(WINAPI * DwmSetAttrFn)(HWND, DWORD, LPCVOID, DWORD);
        DwmSetAttrFn setAttr = reinterpret_cast<DwmSetAttrFn>(
            reinterpret_cast<void *>(GetProcAddress(dwm, "DwmSetWindowAttribute")));
        if (setAttr) {
            BOOL dark = TRUE;
            setAttr(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
            DWORD roundPref = 2 /* DWMWCP_ROUND */;
            setAttr(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &roundPref, sizeof(roundPref));
        }
        FreeLibrary(dwm);
    }
}

static void BoxBlurHorizontal(const BYTE *src, BYTE *dst, int w, int h, int radius) {
    int diam = radius * 2 + 1;
    for (int y = 0; y < h; ++y) {
        const BYTE *rowS = src + static_cast<size_t>(y) * static_cast<size_t>(w) * 4;
        BYTE *rowD = dst + static_cast<size_t>(y) * static_cast<size_t>(w) * 4;
        int sumB = 0, sumG = 0, sumR = 0;
        for (int k = -radius; k <= radius; ++k) {
            int cx = Clamp(k, 0, w - 1);
            sumB += rowS[cx * 4 + 0];
            sumG += rowS[cx * 4 + 1];
            sumR += rowS[cx * 4 + 2];
        }
        for (int x = 0; x < w; ++x) {
            rowD[x * 4 + 0] = static_cast<BYTE>(sumB / diam);
            rowD[x * 4 + 1] = static_cast<BYTE>(sumG / diam);
            rowD[x * 4 + 2] = static_cast<BYTE>(sumR / diam);
            rowD[x * 4 + 3] = 255;

            int outX = Clamp(x - radius, 0, w - 1);
            int inX = Clamp(x + radius + 1, 0, w - 1);
            sumB += static_cast<int>(rowS[inX * 4 + 0]) - static_cast<int>(rowS[outX * 4 + 0]);
            sumG += static_cast<int>(rowS[inX * 4 + 1]) - static_cast<int>(rowS[outX * 4 + 1]);
            sumR += static_cast<int>(rowS[inX * 4 + 2]) - static_cast<int>(rowS[outX * 4 + 2]);
        }
    }
}

static void BoxBlurVertical(const BYTE *src, BYTE *dst, int w, int h, int radius) {
    int diam = radius * 2 + 1;
    size_t stride = static_cast<size_t>(w) * 4;
    for (int x = 0; x < w; ++x) {
        int sumB = 0, sumG = 0, sumR = 0;
        for (int k = -radius; k <= radius; ++k) {
            int cy = Clamp(k, 0, h - 1);
            const BYTE *p = src + static_cast<size_t>(cy) * stride + x * 4;
            sumB += p[0];
            sumG += p[1];
            sumR += p[2];
        }
        for (int y = 0; y < h; ++y) {
            BYTE *d = dst + static_cast<size_t>(y) * stride + x * 4;
            d[0] = static_cast<BYTE>(sumB / diam);
            d[1] = static_cast<BYTE>(sumG / diam);
            d[2] = static_cast<BYTE>(sumR / diam);
            d[3] = 255;

            int outY = Clamp(y - radius, 0, h - 1);
            int inY = Clamp(y + radius + 1, 0, h - 1);
            const BYTE *pOut = src + static_cast<size_t>(outY) * stride + x * 4;
            const BYTE *pIn = src + static_cast<size_t>(inY) * stride + x * 4;
            sumB += static_cast<int>(pIn[0]) - static_cast<int>(pOut[0]);
            sumG += static_cast<int>(pIn[1]) - static_cast<int>(pOut[1]);
            sumR += static_cast<int>(pIn[2]) - static_cast<int>(pOut[2]);
        }
    }
}

void BlurDcRegion(HDC dc, const RECT &rect, int blurRadius) {
    int w = RectWidth(rect);
    int h = RectHeight(rect);
    if (!dc || w <= 2 || h <= 2 || blurRadius <= 0) return;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HBITMAP dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!dib || !bits) {
        if (dib) DeleteObject(dib);
        return;
    }

    HDC memDc = CreateCompatibleDC(dc);
    HGDIOBJ oldObj = SelectObject(memDc, dib);
    BitBlt(memDc, 0, 0, w, h, dc, rect.left, rect.top, SRCCOPY);
    GdiFlush();

    BYTE *bufA = static_cast<BYTE *>(bits);
    std::vector<BYTE> temp(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    int r = Clamp(blurRadius, 2, 32);

    // 2-pass separable box blur approximates a smooth Gaussian kernel in linear time
    BoxBlurHorizontal(bufA, temp.data(), w, h, r);
    BoxBlurVertical(temp.data(), bufA, w, h, r);
    BoxBlurHorizontal(bufA, temp.data(), w, h, std::max(1, r / 2));
    BoxBlurVertical(temp.data(), bufA, w, h, std::max(1, r / 2));

    BitBlt(dc, rect.left, rect.top, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldObj);
    DeleteDC(memDc);
    DeleteObject(dib);
}

void DrawGlassPanel(HDC dc, const RECT &rect, float radius, bool blurBackdrop, bool highlight) {
    int w = RectWidth(rect);
    int h = RectHeight(rect);
    if (w <= 0 || h <= 0) return;

    if (!StartGdiplus()) {
        FillRectColor(dc, rect, RGB(22, 26, 36));
        return;
    }

    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    float fx = static_cast<float>(rect.left);
    float fy = static_cast<float>(rect.top);
    float fw = static_cast<float>(w);
    float fh = static_cast<float>(h);

    GraphicsPath clipPath;
    BuildRoundedRectPath(&clipPath, fx, fy, fw, fh, radius);

    // Soft outer ambient shadow for floating Apple glass elevation
    if (radius > 0.5f) {
        GraphicsPath shadowPath;
        BuildRoundedRectPath(&shadowPath, fx - 1.0f, fy + 1.5f, fw + 2.0f, fh + 2.0f, radius + 1.0f);
        Pen shadowPen(Color(75, 0, 0, 0), 2.0f);
        graphics.DrawPath(&shadowPen, &shadowPath);
    }

    if (blurBackdrop) {
        Gdiplus::Region oldClip;
        graphics.GetClip(&oldClip);
        graphics.SetClip(&clipPath);
        HDC clipDc = graphics.GetHDC();
        BlurDcRegion(clipDc, rect, 12);
        graphics.ReleaseHDC(clipDc);
        graphics.SetClip(&oldClip);
    }

    // Apple macOS Dark Frosted Vibrancy Tint (translucent slate-indigo + milky frost sheen)
    Gdiplus::LinearGradientBrush glassFill(
        PointF(fx, fy), PointF(fx, fy + fh),
        highlight ? Color(160, 44, 52, 78) : Color(152, 28, 33, 46),
        highlight ? Color(175, 28, 34, 54) : Color(172, 16, 20, 30));
    graphics.FillPath(&glassFill, &clipPath);

    // Upper frosted specular sheen (simulates sky light reflection on frosted glass)
    GraphicsPath sheenPath;
    BuildRoundedRectPath(&sheenPath, fx + 1.0f, fy + 1.0f, fw - 2.0f, std::max(4.0f, fh * 0.48f),
                         std::max(1.0f, radius - 1.0f));
    Gdiplus::LinearGradientBrush sheenBrush(
        PointF(fx, fy), PointF(fx, fy + std::max(4.0f, fh * 0.50f)),
        Color(highlight ? 44 : 28, 255, 255, 255),
        Color(0, 255, 255, 255));
    graphics.FillPath(&sheenBrush, &sheenPath);

    // 1px Specular Glass Rim (bright top edge fading to soft bottom edge)
    if (radius > 0.5f) {
        Gdiplus::LinearGradientBrush borderGrad(
            PointF(fx, fy), PointF(fx, fy + fh),
            highlight ? Color(180, 145, 165, 255) : Color(125, 255, 255, 255),
            highlight ? Color(95, 99, 102, 241) : Color(38, 255, 255, 255));
        Pen borderPen(&borderGrad, 1.1f);
        graphics.DrawPath(&borderPen, &clipPath);
    } else {
        Pen topLine(Color(95, 255, 255, 255), 1.0f);
        Pen botLine(Color(55, 255, 255, 255), 1.0f);
        graphics.DrawLine(&topLine, fx, fy, fx + fw, fy);
        graphics.DrawLine(&botLine, fx, fy + fh - 1.0f, fx + fw, fy + fh - 1.0f);
    }
}

void DrawGlassButton(HDC dc, const RECT &rect, float radius, bool primary, bool active,
                     bool hovered, bool pressed, bool blurBackdrop) {
    int w = RectWidth(rect);
    int h = RectHeight(rect);
    if (w <= 0 || h <= 0) return;

    if (!StartGdiplus()) {
        FillRectColor(dc, rect, (primary || active) ? RGB(56, 116, 246) : RGB(32, 38, 52));
        return;
    }

    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    float fx = static_cast<float>(rect.left);
    float fy = static_cast<float>(rect.top);
    float fw = static_cast<float>(w);
    float fh = static_cast<float>(h);

    GraphicsPath path;
    BuildRoundedRectPath(&path, fx, fy, fw, fh, radius);

    if (blurBackdrop) {
        Gdiplus::Region oldClip;
        graphics.GetClip(&oldClip);
        graphics.SetClip(&path);
        HDC clipDc = graphics.GetHDC();
        BlurDcRegion(clipDc, rect, 10);
        graphics.ReleaseHDC(clipDc);
        graphics.SetClip(&oldClip);
    }

    Color topColor, botColor, rimTop, rimBot;
    int sheenAlpha = 36;

    if (primary) {
        // Apple System Blue / Indigo Translucent Glass Pill
        if (pressed) {
            topColor = Color(210, 46, 98, 226);
            botColor = Color(220, 34, 76, 198);
            sheenAlpha = 22;
        } else if (hovered) {
            topColor = Color(215, 86, 148, 255);
            botColor = Color(205, 58, 108, 246);
            sheenAlpha = 64;
        } else {
            topColor = Color(198, 66, 130, 252);
            botColor = Color(190, 48, 96, 236);
            sheenAlpha = 48;
        }
        rimTop = Color(195, 200, 228, 255);
        rimBot = Color(95, 92, 148, 255);
    } else if (active) {
        // Active Tool / Mode Pill in Frosted Luminous Glass
        if (hovered) {
            topColor = Color(175, 88, 128, 248);
            botColor = Color(155, 62, 92, 218);
            sheenAlpha = 58;
        } else {
            topColor = Color(155, 76, 114, 238);
            botColor = Color(135, 52, 82, 204);
            sheenAlpha = 46;
        }
        rimTop = Color(185, 185, 215, 255);
        rimBot = Color(90, 105, 175, 255);
    } else {
        // Neutral Apple Frosted Glass Capsule
        if (pressed) {
            topColor = Color(85, 255, 255, 255);
            botColor = Color(48, 255, 255, 255);
            sheenAlpha = 20;
            rimTop = Color(135, 255, 255, 255);
            rimBot = Color(55, 255, 255, 255);
        } else if (hovered) {
            topColor = Color(68, 255, 255, 255);
            botColor = Color(32, 255, 255, 255);
            sheenAlpha = 45;
            rimTop = Color(150, 255, 255, 255);
            rimBot = Color(60, 255, 255, 255);
        } else {
            topColor = Color(36, 255, 255, 255);
            botColor = Color(15, 255, 255, 255);
            sheenAlpha = 26;
            rimTop = Color(96, 255, 255, 255);
            rimBot = Color(32, 255, 255, 255);
        }
    }

    // Drop shadow under button
    GraphicsPath shadowPath;
    BuildRoundedRectPath(&shadowPath, fx, fy + 1.0f, fw, fh, radius);
    Pen shadowPen(Color(55, 0, 0, 0), 1.2f);
    graphics.DrawPath(&shadowPen, &shadowPath);

    // Main frosted glass gradient fill
    Gdiplus::LinearGradientBrush fillGrad(PointF(fx, fy), PointF(fx, fy + fh), topColor, botColor);
    graphics.FillPath(&fillGrad, &path);

    // Top specular glass highlight
    if (fh >= 12.0f) {
        GraphicsPath highlightPath;
        BuildRoundedRectPath(&highlightPath, fx + 1.0f, fy + 1.0f, fw - 2.0f, fh * 0.48f,
                             std::max(1.0f, radius - 1.0f));
        Gdiplus::LinearGradientBrush sheenGrad(
            PointF(fx, fy), PointF(fx, fy + fh * 0.5f),
            Color(sheenAlpha, 255, 255, 255), Color(0, 255, 255, 255));
        graphics.FillPath(&sheenGrad, &highlightPath);
    }

    // Specular 1px glass rim
    Gdiplus::LinearGradientBrush rimGrad(PointF(fx, fy), PointF(fx, fy + fh), rimTop, rimBot);
    Pen rimPen(&rimGrad, (primary || active) ? 1.2f : 1.0f);
    graphics.DrawPath(&rimPen, &path);
}

void DrawToolbarIcon(HDC dc, Action action, const RECT &button, bool active) {
    COLORREF color = active ? RGB(255, 255, 255) : RGB(236, 242, 250);
    float x = static_cast<float>(button.left);
    float y = static_cast<float>(button.top);
    float w = static_cast<float>(RectWidth(button));

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        Color iconColor(255, GetRValue(color), GetGValue(color), GetBValue(color));
        Pen pen(iconColor, 2.0f);
        SolidBrush brush(iconColor);
        float cx = x + w / 2.0f;
        float top = y + 8.0f;

        if (action == ActionToolBlur) {
            float s = 5.5f;
            float gap = 2.0f;
            float startX = cx - s - gap / 2.0f;
            float startY = top + 3.0f;
            graphics.FillRectangle(&brush, startX, startY, s, s);
            graphics.FillRectangle(&brush, startX + s + gap, startY, s, s);
            graphics.FillRectangle(&brush, startX, startY + s + gap, s, s);
            graphics.FillRectangle(&brush, startX + s + gap, startY + s + gap, s, s);
        } else if (action == ActionToolArrow) {
            Pen shaft(iconColor, 2.2f);
            shaft.SetEndCap(Gdiplus::LineCapRound);
            shaft.SetStartCap(Gdiplus::LineCapRound);
            graphics.DrawLine(&shaft, cx - 9.0f, top + 15.0f, cx + 9.0f, top + 4.0f);
            PointF head[3] = {
                PointF(cx + 10.0f, top + 3.0f),
                PointF(cx + 2.5f, top + 5.0f),
                PointF(cx + 7.5f, top + 10.5f)
            };
            graphics.FillPolygon(&brush, head, 3);
        } else if (action == ActionToolRect) {
            GraphicsPath rPath;
            BuildRoundedRectPath(&rPath, cx - 10.5f, top + 4.0f, 21.0f, 14.0f, 3.0f);
            graphics.DrawPath(&pen, &rPath);
        } else if (action == ActionUndo) {
            Font glyph(L"Segoe MDL2 Assets", 18.0f, FontStyleRegular, UnitPixel);
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            RectF target(x, top - 1.0f, w, 22.0f);
            graphics.DrawString(L"", -1, &glyph, target, &fmt, &brush);
        } else if (action == ActionCopy) {
            Font glyph(L"Segoe MDL2 Assets", 16.5f, FontStyleRegular, UnitPixel);
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            RectF target(x, top, w, 22.0f);
            graphics.DrawString(L"", -1, &glyph, target, &fmt, &brush);
        } else if (action == ActionSave) {
            Font glyph(L"Segoe MDL2 Assets", 17.5f, FontStyleRegular, UnitPixel);
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            RectF target(x, top - 1.0f, w, 22.0f);
            graphics.DrawString(L"", -1, &glyph, target, &fmt, &brush);
        } else {
            Font font(L"Segoe UI", 20.0f, FontStyleBold, UnitPixel);
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            RectF target(x, top - 2.0f, w, 22.0f);
            graphics.DrawString(L"T", -1, &font, target, &fmt, &brush);
        }
        return;
    }

    RECT icon = {button.left + 10, button.top + 6, button.right - 10, button.top + 30};
    if (action == ActionToolText) {
        DrawCenteredText(dc, L"T", icon, -24, FW_SEMIBOLD, color);
    } else {
        HPEN pen = CreatePen(PS_SOLID, 2, color);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, icon.left + 5, icon.top + 5, icon.right - 5, icon.bottom - 5);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
}

void ApplyMosaic(HDC dc, RECT rect) {
    const int block = 12;
    if (RectWidth(rect) <= 0 || RectHeight(rect) <= 0) {
        return;
    }

    for (int y = rect.top; y < rect.bottom; y += block) {
        int bh = std::min<int>(block, rect.bottom - y);
        for (int x = rect.left; x < rect.right; x += block) {
            int bw = std::min<int>(block, rect.right - x);
            COLORREF color = GetPixel(dc, x + bw / 2, y + bh / 2);
            RECT tile = {x, y, x + bw, y + bh};
            FillRectColor(dc, tile, color);
        }
    }
}

void DrawArrow(HDC dc, POINT a, POINT b, COLORREF color) {
    double dx = static_cast<double>(b.x - a.x);
    double dy = static_cast<double>(b.y - a.y);
    double len = std::sqrt(dx * dx + dy * dy);
    if (len < 2.0) {
        return;
    }

    double angle = std::atan2(dy, dx);
    double headLen = 16.0;
    double headAngle = 0.55;
    POINT h1 = {b.x - static_cast<int>(std::cos(angle - headAngle) * headLen),
                b.y - static_cast<int>(std::sin(angle - headAngle) * headLen)};
    POINT h2 = {b.x - static_cast<int>(std::cos(angle + headAngle) * headLen),
                b.y - static_cast<int>(std::sin(angle + headAngle) * headLen)};

    HPEN pen = CreatePen(PS_SOLID, 4, color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, a.x, a.y, NULL);
    LineTo(dc, b.x, b.y);
    LineTo(dc, h1.x, h1.y);
    MoveToEx(dc, b.x, b.y, NULL);
    LineTo(dc, h2.x, h2.y);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void DrawAnnotation(HDC dc, const Annotation &op, int offsetX, int offsetY, int maxW, int maxH) {
    POINT a = {op.a.x + offsetX, op.a.y + offsetY};
    POINT b = {op.b.x + offsetX, op.b.y + offsetY};

    if (op.tool == ToolBlur) {
        RECT rect = NormalizeRectPoints(a, b);
        rect = ClampRect(rect, offsetX, offsetY, offsetX + maxW, offsetY + maxH);
        ApplyMosaic(dc, rect);
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 207, 90));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    } else if (op.tool == ToolRect) {
        RECT rect = NormalizeRectPoints(a, b);
        rect = ClampRect(rect, offsetX, offsetY, offsetX + maxW, offsetY + maxH);
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(239, 68, 68));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    } else if (op.tool == ToolArrow) {
        DrawArrow(dc, a, b, RGB(245, 158, 11));
    } else if (op.tool == ToolText) {
        SIZE size;
        HFONT font = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(dc, font);
        GetTextExtentPoint32W(dc, op.text.c_str(), static_cast<int>(op.text.size()), &size);
        RECT bg = {a.x - 8, a.y - size.cy - 6, a.x + size.cx + 10, a.y + 6};
        DrawGlassPanel(dc, bg, 8.0f, true, false);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(248, 250, 252));
        TextOutW(dc, a.x, a.y - size.cy, op.text.c_str(), static_cast<int>(op.text.size()));
        SelectObject(dc, oldFont);
        DeleteObject(font);
    }
}

} // namespace ModernScreenshot
