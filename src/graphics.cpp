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

HCURSOR CreateAeroGlassCursor(bool interactiveOrb) {
    if (!StartGdiplus()) {
        return LoadCursor(NULL, interactiveOrb ? IDC_HAND : IDC_ARROW);
    }

    const int size = 32;
    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = size;
    bi.bV5Height = -size;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    HDC screen = GetDC(NULL);
    void *bits = NULL;
    HBITMAP colorBmp = CreateDIBSection(screen, reinterpret_cast<BITMAPINFO *>(&bi),
                                        DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (!colorBmp || !bits) {
        if (colorBmp) DeleteObject(colorBmp);
        return LoadCursor(NULL, interactiveOrb ? IDC_HAND : IDC_ARROW);
    }

    std::memset(bits, 0, static_cast<size_t>(size) * static_cast<size_t>(size) * 4);

    {
        Bitmap canvas(size, size, size * 4, PixelFormat32bppARGB, static_cast<BYTE *>(bits));
        Graphics g(&canvas);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        PointF arrow[7] = {
            PointF(3.0f, 3.0f),
            PointF(3.0f, 21.5f),
            PointF(7.8f, 17.0f),
            PointF(11.2f, 24.5f),
            PointF(14.2f, 23.1f),
            PointF(10.8f, 15.6f),
            PointF(17.2f, 15.6f)
        };

        PointF shadowPts[7];
        for (int i = 0; i < 7; ++i) {
            shadowPts[i] = PointF(arrow[i].X + 1.5f, arrow[i].Y + 2.0f);
        }

        // 1. Soft Aero drop shadow
        GraphicsPath shadowPath;
        shadowPath.AddPolygon(shadowPts, 7);
        SolidBrush shadowFill(Color(95, 0, 0, 0));
        Pen shadowPen(Color(55, 0, 0, 0), 2.0f);
        shadowPen.SetLineJoin(Gdiplus::LineJoinRound);
        g.FillPath(&shadowFill, &shadowPath);
        g.DrawPath(&shadowPen, &shadowPath);

        // 2. Windows 7 Aero Glass Arrow Body
        GraphicsPath arrowPath;
        arrowPath.AddPolygon(arrow, 7);

        Gdiplus::LinearGradientBrush glassGrad(
            PointF(3.0f, 3.0f), PointF(17.5f, 24.5f),
            interactiveOrb ? Color(250, 210, 242, 255) : Color(245, 250, 254, 255),
            interactiveOrb ? Color(245, 24, 148, 255) : Color(232, 130, 206, 252));
        g.FillPath(&glassGrad, &arrowPath);

        // 3. Signature Win7 Aero split-facet crystal reflection on left half of arrow
        PointF facet[3] = {
            PointF(3.8f, 4.8f),
            PointF(3.8f, 19.2f),
            PointF(10.2f, 14.4f)
        };
        Gdiplus::LinearGradientBrush facetGrad(
            PointF(3.8f, 4.5f), PointF(10.2f, 19.2f),
            Color(200, 255, 255, 255),
            Color(30, 255, 255, 255));
        g.FillPolygon(&facetGrad, facet, 3);

        // 4. Inner white glass bevel + crisp dark Aero contour
        Pen innerBevel(Color(210, 255, 255, 255), 1.6f);
        innerBevel.SetLineJoin(Gdiplus::LineJoinRound);
        g.DrawPath(&innerBevel, &arrowPath);

        Pen outerContour(Color(240, 20, 38, 62), 1.2f);
        outerContour.SetLineJoin(Gdiplus::LineJoinRound);
        g.DrawPath(&outerContour, &arrowPath);

        // 5. Windows 7 Aero Glass Interactive Blue Orb (shown when hovering clickable mode buttons)
        if (interactiveOrb) {
            float ox = 15.5f, oy = 4.0f, od = 13.0f;
            SolidBrush orbShadow(Color(90, 0, 0, 0));
            g.FillEllipse(&orbShadow, ox + 1.0f, oy + 1.5f, od, od);

            Gdiplus::LinearGradientBrush orbGrad(
                PointF(ox, oy), PointF(ox + od, oy + od),
                Color(250, 125, 228, 255),
                Color(248, 0, 114, 226));
            g.FillEllipse(&orbGrad, ox, oy, od, od);

            // Inner cyan core glow
            SolidBrush coreGlow(Color(110, 186, 240, 255));
            g.FillEllipse(&coreGlow, ox + 2.5f, oy + 5.5f, od - 5.0f, od - 6.5f);

            // Upper glass crescent specular shine
            Gdiplus::LinearGradientBrush crescentGrad(
                PointF(ox, oy + 1.0f), PointF(ox, oy + 6.5f),
                Color(220, 255, 255, 255),
                Color(15, 255, 255, 255));
            g.FillEllipse(&crescentGrad, ox + 2.2f, oy + 1.2f, od - 4.4f, 5.2f);

            Pen orbRim(Color(235, 224, 246, 255), 1.1f);
            g.DrawEllipse(&orbRim, ox, oy, od, od);
            Pen orbOuter(Color(200, 14, 72, 140), 0.9f);
            g.DrawEllipse(&orbOuter, ox - 0.3f, oy - 0.3f, od + 0.6f, od + 0.6f);
        }
    }

    std::vector<BYTE> maskBits(size * size / 8, 0);
    HBITMAP maskBmp = CreateBitmap(size, size, 1, 1, maskBits.data());

    ICONINFO ii = {};
    ii.fIcon = FALSE;
    ii.xHotspot = 3;
    ii.yHotspot = 3;
    ii.hbmMask = maskBmp;
    ii.hbmColor = colorBmp;

    HCURSOR cursor = reinterpret_cast<HCURSOR>(CreateIconIndirect(&ii));
    DeleteObject(colorBmp);
    DeleteObject(maskBmp);
    return cursor ? cursor : LoadCursor(NULL, interactiveOrb ? IDC_HAND : IDC_ARROW);
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

    // Apple macOS Dark Smoked-Glass Vibrancy Tint (high contrast even over white backgrounds)
    Gdiplus::LinearGradientBrush glassFill(
        PointF(fx, fy), PointF(fx, fy + fh),
        highlight ? Color(210, 26, 34, 54) : Color(202, 15, 20, 32),
        highlight ? Color(225, 18, 24, 40) : Color(218, 10, 14, 22));
    graphics.FillPath(&glassFill, &clipPath);

    // Subtle upper specular glass sheen
    GraphicsPath sheenPath;
    BuildRoundedRectPath(&sheenPath, fx + 1.0f, fy + 1.0f, fw - 2.0f, std::max(4.0f, fh * 0.42f),
                         std::max(1.0f, radius - 1.0f));
    Gdiplus::LinearGradientBrush sheenBrush(
        PointF(fx, fy), PointF(fx, fy + std::max(4.0f, fh * 0.45f)),
        Color(highlight ? 32 : 20, 255, 255, 255),
        Color(0, 255, 255, 255));
    graphics.FillPath(&sheenBrush, &sheenPath);

    // Dual-layer Apple Glass Border (outer luminous accent + inner specular rim)
    if (radius > 0.5f) {
        Gdiplus::LinearGradientBrush borderGrad(
            PointF(fx, fy), PointF(fx, fy + fh),
            highlight ? Color(230, 96, 165, 250) : Color(150, 148, 163, 184),
            highlight ? Color(165, 59, 130, 246) : Color(75, 100, 116, 139));
        Pen borderPen(&borderGrad, highlight ? 1.4f : 1.1f);
        graphics.DrawPath(&borderPen, &clipPath);
    } else {
        Pen topLine(Color(95, 148, 163, 184), 1.0f);
        Pen botLine(Color(110, 56, 70, 98), 1.0f);
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
        FillRectColor(dc, rect, (primary || active) ? RGB(24, 118, 252) : RGB(20, 26, 38));
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
    int sheenAlpha = 28;

    if (primary || active) {
        // Vibrant Apple Electric Blue Glass (unmistakably distinct from unselected buttons)
        if (pressed) {
            topColor = Color(245, 16, 92, 216);
            botColor = Color(250, 12, 72, 188);
            sheenAlpha = 18;
        } else if (hovered) {
            topColor = Color(248, 42, 138, 255);
            botColor = Color(248, 20, 102, 236);
            sheenAlpha = 48;
        } else {
            topColor = Color(242, 24, 118, 252);
            botColor = Color(246, 14, 88, 224);
            sheenAlpha = 36;
        }
        rimTop = Color(245, 147, 210, 255);
        rimBot = Color(180, 56, 138, 248);
    } else {
        // Dark Smoked-Obsidian Frosted Glass (never washes out to white)
        if (pressed) {
            topColor = Color(230, 34, 46, 72);
            botColor = Color(238, 24, 34, 54);
            sheenAlpha = 16;
            rimTop = Color(190, 96, 165, 250);
            rimBot = Color(120, 59, 130, 246);
        } else if (hovered) {
            topColor = Color(222, 38, 52, 82);
            botColor = Color(232, 26, 36, 58);
            sheenAlpha = 30;
            rimTop = Color(215, 125, 211, 252);
            rimBot = Color(135, 59, 130, 246);
        } else {
            topColor = Color(205, 20, 26, 40);
            botColor = Color(220, 13, 17, 28);
            sheenAlpha = 16;
            rimTop = Color(105, 148, 163, 184);
            rimBot = Color(55, 71, 85, 105);
        }
    }

    // Drop shadow under button
    GraphicsPath shadowPath;
    BuildRoundedRectPath(&shadowPath, fx, fy + 1.0f, fw, fh, radius);
    Pen shadowPen(Color(80, 0, 0, 0), 1.2f);
    graphics.DrawPath(&shadowPen, &shadowPath);

    // Main frosted glass gradient fill
    Gdiplus::LinearGradientBrush fillGrad(PointF(fx, fy), PointF(fx, fy + fh), topColor, botColor);
    graphics.FillPath(&fillGrad, &path);

    // Top specular glass highlight
    if (fh >= 12.0f) {
        GraphicsPath highlightPath;
        BuildRoundedRectPath(&highlightPath, fx + 1.0f, fy + 1.0f, fw - 2.0f, fh * 0.42f,
                             std::max(1.0f, radius - 1.0f));
        Gdiplus::LinearGradientBrush sheenGrad(
            PointF(fx, fy), PointF(fx, fy + fh * 0.45f),
            Color(sheenAlpha, 255, 255, 255), Color(0, 255, 255, 255));
        graphics.FillPath(&sheenGrad, &highlightPath);
    }

    // Specular 1px glass rim
    Gdiplus::LinearGradientBrush rimGrad(PointF(fx, fy), PointF(fx, fy + fh), rimTop, rimBot);
    Pen rimPen(&rimGrad, (primary || active || hovered) ? 1.3f : 1.0f);
    graphics.DrawPath(&rimPen, &path);

    // Glowing bottom indicator pill on active tool/mode buttons
    if (active && fw >= 36.0f && fh >= 28.0f) {
        float indW = std::min(22.0f, fw * 0.36f);
        float indX = fx + (fw - indW) * 0.5f;
        float indY = fy + fh - 3.2f;
        SolidBrush indBrush(Color(245, 186, 230, 255));
        graphics.FillRectangle(&indBrush, indX, indY, indW, 2.0f);
    }
}

void DrawToolbarIcon(HDC dc, Action action, const RECT &button, bool active) {
    COLORREF color = RGB(255, 255, 255);
    if (!active) {
        switch (action) {
        case ActionToolBlur:  color = RGB(251, 191, 36); break;  // Amber Gold
        case ActionToolArrow: color = RGB(251, 146, 60); break;  // Vibrant Orange
        case ActionToolRect:  color = RGB(248, 113, 113); break; // Coral Rose
        case ActionToolText:  color = RGB(56, 189, 248); break;  // Sky Cyan
        case ActionUndo:      color = RGB(203, 213, 225); break; // Cool Silver
        case ActionCopy:      color = RGB(52, 211, 153); break;  // Emerald Green
        case ActionSave:      color = RGB(96, 165, 250); break;  // Electric Blue
        default:              color = RGB(226, 232, 240); break;
        }
    }
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
    int w = RectWidth(rect);
    int h = RectHeight(rect);
    if (!dc || w <= 0 || h <= 0) {
        return;
    }

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

    BYTE *pixels = static_cast<BYTE *>(bits);
    const int block = 10;

    for (int by = 0; by < h; by += block) {
        int bh = std::min(block, h - by);
        for (int bx = 0; bx < w; bx += block) {
            int bw = std::min(block, w - bx);
            int sumB = 0, sumG = 0, sumR = 0, count = 0;

            for (int y = by; y < by + bh; ++y) {
                const BYTE *row = pixels + static_cast<size_t>(y) * static_cast<size_t>(w) * 4;
                for (int x = bx; x < bx + bw; ++x) {
                    sumB += row[x * 4 + 0];
                    sumG += row[x * 4 + 1];
                    sumR += row[x * 4 + 2];
                    ++count;
                }
            }

            int avgB = count > 0 ? (sumB / count) : 0;
            int avgG = count > 0 ? (sumG / count) : 0;
            int avgR = count > 0 ? (sumR / count) : 0;

            for (int y = by; y < by + bh; ++y) {
                BYTE *row = pixels + static_cast<size_t>(y) * static_cast<size_t>(w) * 4;
                bool isTop = (y == by);
                bool isBot = (y == by + bh - 1);
                for (int x = bx; x < bx + bw; ++x) {
                    bool isLeft = (x == bx);
                    bool isRight = (x == bx + bw - 1);
                    // Subtle frosted crystal micro-bevel on each mosaic tile
                    int delta = (isTop || isLeft) ? 6 : ((isBot || isRight) ? -6 : 0);
                    row[x * 4 + 0] = static_cast<BYTE>(Clamp(avgB + delta, 0, 255));
                    row[x * 4 + 1] = static_cast<BYTE>(Clamp(avgG + delta, 0, 255));
                    row[x * 4 + 2] = static_cast<BYTE>(Clamp(avgR + delta, 0, 255));
                    row[x * 4 + 3] = 255;
                }
            }
        }
    }

    BitBlt(dc, rect.left, rect.top, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldObj);
    DeleteDC(memDc);
    DeleteObject(dib);
}

static void DrawGlassSelectionHandles(Graphics *graphics, float left, float top, float right, float bottom) {
    PointF corners[4] = {
        PointF(left, top),
        PointF(right, top),
        PointF(right, bottom),
        PointF(left, bottom)
    };
    const float r = 4.0f;
    SolidBrush fill(Color(255, 255, 255, 255));
    Pen ring(Color(255, 24, 118, 252), 1.6f);
    Pen shadow(Color(120, 0, 0, 0), 1.2f);
    for (int i = 0; i < 4; ++i) {
        graphics->DrawEllipse(&shadow, corners[i].X - r - 0.5f, corners[i].Y - r - 0.5f, r * 2.0f + 1.0f, r * 2.0f + 1.0f);
        graphics->FillEllipse(&fill, corners[i].X - r, corners[i].Y - r, r * 2.0f, r * 2.0f);
        graphics->DrawEllipse(&ring, corners[i].X - r, corners[i].Y - r, r * 2.0f, r * 2.0f);
    }
}

void DrawArrow(HDC dc, POINT a, POINT b, COLORREF color) {
    (void)color;
    float ax = static_cast<float>(a.x);
    float ay = static_cast<float>(a.y);
    float bx = static_cast<float>(b.x);
    float by = static_cast<float>(b.y);
    float dx = bx - ax;
    float dy = by - ay;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 3.0f) {
        return;
    }

    if (!StartGdiplus()) {
        HPEN pen = CreatePen(PS_SOLID, 3, RGB(56, 189, 248));
        HGDIOBJ old = SelectObject(dc, pen);
        MoveToEx(dc, a.x, a.y, NULL);
        LineTo(dc, b.x, b.y);
        SelectObject(dc, old);
        DeleteObject(pen);
        return;
    }

    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    float ux = dx / len;
    float uy = dy / len;
    float nx = -uy;
    float ny = ux;

    float headLen = std::min(22.0f, std::max(12.0f, len * 0.28f));
    float neckDist = headLen * 0.72f;
    float wingHalfW = headLen * 0.46f;
    float tailHalfW = 1.6f;
    float neckHalfW = std::min(4.0f, headLen * 0.19f);

    float neckX = bx - ux * neckDist;
    float neckY = by - uy * neckDist;
    float barbX = bx - ux * headLen;
    float barbY = by - uy * headLen;

    PointF poly[7] = {
        PointF(ax + nx * tailHalfW, ay + ny * tailHalfW),
        PointF(neckX + nx * neckHalfW, neckY + ny * neckHalfW),
        PointF(barbX + nx * wingHalfW, barbY + ny * wingHalfW),
        PointF(bx, by),
        PointF(barbX - nx * wingHalfW, barbY - ny * wingHalfW),
        PointF(neckX - nx * neckHalfW, neckY - ny * neckHalfW),
        PointF(ax - nx * tailHalfW, ay - ny * tailHalfW)
    };

    PointF shadowPoly[7];
    for (int i = 0; i < 7; ++i) {
        shadowPoly[i] = PointF(poly[i].X + 1.5f, poly[i].Y + 2.0f);
    }

    GraphicsPath shadowPath;
    shadowPath.AddPolygon(shadowPoly, 7);
    SolidBrush shadowBrush(Color(110, 0, 0, 0));
    Pen shadowPen(Color(65, 0, 0, 0), 2.0f);
    shadowPen.SetLineJoin(Gdiplus::LineJoinRound);
    graphics.FillPath(&shadowBrush, &shadowPath);
    graphics.DrawPath(&shadowPen, &shadowPath);

    GraphicsPath arrowPath;
    arrowPath.AddPolygon(poly, 7);

    // Apple Electric Sky-Cyan to System Blue gradient fill
    Gdiplus::LinearGradientBrush arrowFill(
        PointF(ax, ay), PointF(bx, by),
        Color(250, 56, 189, 248),
        Color(255, 24, 118, 252));
    graphics.FillPath(&arrowFill, &arrowPath);

    // Crisp luminous white-cyan glass specular outline
    Pen specularRim(Color(230, 224, 242, 254), 1.1f);
    specularRim.SetLineJoin(Gdiplus::LineJoinRound);
    graphics.DrawPath(&specularRim, &arrowPath);
}

void DrawAnnotation(HDC dc, const Annotation &op, int offsetX, int offsetY, int maxW, int maxH, bool isPreview) {
    POINT a = {op.a.x + offsetX, op.a.y + offsetY};
    POINT b = {op.b.x + offsetX, op.b.y + offsetY};

    if (op.tool == ToolBlur) {
        RECT rect = NormalizeRectPoints(a, b);
        rect = ClampRect(rect, offsetX, offsetY, offsetX + maxW, offsetY + maxH);
        int rw = RectWidth(rect);
        int rh = RectHeight(rect);
        if (rw <= 0 || rh <= 0) return;

        ApplyMosaic(dc, rect);

        if (StartGdiplus()) {
            Graphics graphics(dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            float fx = static_cast<float>(rect.left);
            float fy = static_cast<float>(rect.top);
            float fw = static_cast<float>(rw);
            float fh = static_cast<float>(rh);

            if (isPreview) {
                Pen outerGlow(Color(90, 24, 118, 252), 3.0f);
                Pen guideBorder(Color(240, 56, 189, 248), 1.6f);
                Pen innerRim(Color(150, 255, 255, 255), 1.0f);
                graphics.DrawRectangle(&outerGlow, fx - 1.0f, fy - 1.0f, fw + 2.0f, fh + 2.0f);
                graphics.DrawRectangle(&guideBorder, fx, fy, fw, fh);
                if (rw > 2 && rh > 2) {
                    graphics.DrawRectangle(&innerRim, fx + 1.0f, fy + 1.0f, fw - 2.0f, fh - 2.0f);
                }
                DrawGlassSelectionHandles(&graphics, fx, fy, fx + fw, fy + fh);
            } else {
                // Subtle 1px frosted glass bevel on committed mosaic
                Pen glassEdge(Color(65, 255, 255, 255), 1.0f);
                graphics.DrawRectangle(&glassEdge, fx, fy, fw - 1.0f, fh - 1.0f);
            }
        }
    } else if (op.tool == ToolRect) {
        RECT rect = NormalizeRectPoints(a, b);
        rect = ClampRect(rect, offsetX, offsetY, offsetX + maxW, offsetY + maxH);
        int rw = RectWidth(rect);
        int rh = RectHeight(rect);
        if (rw <= 0 || rh <= 0) return;

        if (StartGdiplus()) {
            Graphics graphics(dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            float fx = static_cast<float>(rect.left);
            float fy = static_cast<float>(rect.top);
            float fw = static_cast<float>(rw);
            float fh = static_cast<float>(rh);
            float rad = std::min(6.0f, std::min(fw, fh) * 0.2f);

            GraphicsPath shadowPath, outerPath, innerPath;
            BuildRoundedRectPath(&shadowPath, fx + 1.0f, fy + 1.5f, fw, fh, rad);
            BuildRoundedRectPath(&outerPath, fx, fy, fw, fh, rad);

            // Soft drop shadow + outer electric blue aura
            Pen darkShadow(Color(115, 0, 0, 0), 3.6f);
            Pen blueAura(Color(85, 24, 118, 252), 4.2f);
            graphics.DrawPath(&darkShadow, &shadowPath);
            graphics.DrawPath(&blueAura, &outerPath);

            // Subtle translucent glass tint inside selection box while dragging
            if (isPreview) {
                SolidBrush previewTint(Color(18, 56, 189, 248));
                graphics.FillPath(&previewTint, &outerPath);
            }

            // Main luminous Sky-Cyan to Electric Blue gradient stroke
            Gdiplus::LinearGradientBrush strokeGrad(
                PointF(fx, fy), PointF(fx + fw, fy + fh),
                Color(255, 56, 189, 248),
                Color(255, 24, 118, 252));
            Pen mainPen(&strokeGrad, 2.2f);
            graphics.DrawPath(&mainPen, &outerPath);

            // 1px inner white specular glass rim
            if (fw > 4.0f && fh > 4.0f) {
                BuildRoundedRectPath(&innerPath, fx + 1.2f, fy + 1.2f, fw - 2.4f, fh - 2.4f, std::max(1.0f, rad - 1.0f));
                Pen innerRim(Color(165, 255, 255, 255), 1.0f);
                graphics.DrawPath(&innerRim, &innerPath);
            }

            if (isPreview) {
                DrawGlassSelectionHandles(&graphics, fx, fy, fx + fw, fy + fh);
            }
        } else {
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(56, 189, 248));
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
        }
    } else if (op.tool == ToolArrow) {
        DrawArrow(dc, a, b, RGB(56, 189, 248));
        if (isPreview && StartGdiplus()) {
            Graphics graphics(dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            SolidBrush fill(Color(255, 255, 255, 255));
            Pen ring(Color(255, 24, 118, 252), 1.5f);
            graphics.FillEllipse(&fill, static_cast<float>(a.x) - 3.5f, static_cast<float>(a.y) - 3.5f, 7.0f, 7.0f);
            graphics.DrawEllipse(&ring, static_cast<float>(a.x) - 3.5f, static_cast<float>(a.y) - 3.5f, 7.0f, 7.0f);
        }
    } else if (op.tool == ToolText) {
        std::wstring displayText = op.text;
        bool isEmptyPlaceholder = false;
        if (isPreview) {
            if (!displayText.empty() && displayText.back() == L'_') {
                displayText.pop_back();
            }
            if (displayText.empty()) {
                displayText = L"Type text...";
                isEmptyPlaceholder = true;
            }
        }
        if (displayText.empty()) return;

        SIZE size = {80, 20};
        HFONT measureFont = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(dc, measureFont);
        GetTextExtentPoint32W(dc, displayText.c_str(), static_cast<int>(displayText.size()), &size);
        SelectObject(dc, oldFont);
        DeleteObject(measureFont);

        int padLeft = 14;
        int padRight = 14;
        int boxH = std::max<int>(30, size.cy + 12);
        int boxW = std::max<int>(44, size.cx + padLeft + padRight);
        int topY = std::max(offsetY + 2, static_cast<int>(a.y - boxH + 6));
        int leftX = std::max(offsetX + 2, static_cast<int>(a.x - 4));
        RECT bg = {leftX, topY, leftX + boxW, topY + boxH};

        DrawGlassPanel(dc, bg, 9.0f, true, isPreview);

        if (StartGdiplus()) {
            Graphics graphics(dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

            // Glowing left vertical sky-cyan accent pill inside the text badge
            GraphicsPath barPath;
            BuildRoundedRectPath(&barPath, static_cast<float>(bg.left + 6), static_cast<float>(bg.top + 6),
                                 3.0f, static_cast<float>(boxH - 12), 1.5f);
            SolidBrush barBrush(Color(255, 56, 189, 248));
            graphics.FillPath(&barBrush, &barPath);

            Font font(L"Segoe UI", 13.5f, isEmptyPlaceholder ? FontStyleRegular : FontStyleBold, UnitPixel);
            SolidBrush textBrush(isEmptyPlaceholder ? Color(185, 186, 204, 226) : Color(255, 255, 255, 255));
            PointF textPt(static_cast<float>(bg.left + padLeft),
                          static_cast<float>(bg.top + (boxH - size.cy) / 2 - 1));
            graphics.DrawString(displayText.c_str(), -1, &font, textPt, &textBrush);

            if (isPreview) {
                float caretX = isEmptyPlaceholder
                                   ? static_cast<float>(bg.left + padLeft)
                                   : static_cast<float>(bg.left + padLeft + size.cx + 2);
                Pen caretPen(Color(255, 56, 189, 248), 2.0f);
                graphics.DrawLine(&caretPen, caretX, static_cast<float>(bg.top + 7),
                                  caretX, static_cast<float>(bg.bottom - 7));
            }
        } else {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(248, 250, 252));
            TextOutW(dc, bg.left + padLeft, bg.top + 6, displayText.c_str(), static_cast<int>(displayText.size()));
        }
    }
}

} // namespace ModernScreenshot
