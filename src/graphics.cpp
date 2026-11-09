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

void DrawToolbarIcon(HDC dc, Action action, const RECT &button, bool active) {
    COLORREF color = active ? RGB(129, 140, 248) : RGB(240, 244, 250);
    float x = static_cast<float>(button.left);
    float y = static_cast<float>(button.top);
    float w = static_cast<float>(RectWidth(button));

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        Color iconColor(255, GetRValue(color), GetGValue(color), GetBValue(color));
        Color muted(170, GetRValue(color), GetGValue(color), GetBValue(color));
        Pen pen(iconColor, 2.0f);
        SolidBrush brush(iconColor);
        float cx = x + w / 2.0f;
        float top = y + 9.0f;

        if (action == ActionToolBlur) {
            float s = 6.0f;
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
            graphics.DrawLine(&shaft, cx - 10.0f, top + 16.0f, cx + 10.0f, top + 4.0f);
            PointF head[3] = {
                PointF(cx + 11.0f, top + 3.0f),
                PointF(cx + 3.0f, top + 5.0f),
                PointF(cx + 8.0f, top + 11.0f)
            };
            graphics.FillPolygon(&brush, head, 3);
        } else if (action == ActionToolRect) {
            graphics.DrawRectangle(&pen, cx - 11.0f, top + 4.0f, 22.0f, 15.0f);
        } else if (action == ActionUndo) {
            Font glyph(L"Segoe MDL2 Assets", 19.0f, FontStyleRegular, UnitPixel);
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            RectF target(x, top - 2.0f, w, 24.0f);
            graphics.DrawString(L"", -1, &glyph, target, &fmt, &brush);
        } else if (action == ActionCopy) {
            Font glyph(L"Segoe MDL2 Assets", 17.0f, FontStyleRegular, UnitPixel);
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            RectF target(x, top - 1.0f, w, 24.0f);
            graphics.DrawString(L"", -1, &glyph, target, &fmt, &brush);
        } else if (action == ActionSave) {
            Font glyph(L"Segoe MDL2 Assets", 18.0f, FontStyleRegular, UnitPixel);
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            RectF target(x, top - 2.0f, w, 24.0f);
            graphics.DrawString(L"", -1, &glyph, target, &fmt, &brush);
        } else {
            Font font(L"Segoe UI", 22.0f, FontStyleBold, UnitPixel);
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            RectF target(x, top - 3.0f, w, 24.0f);
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
        RECT bg = {a.x - 6, a.y - size.cy - 6, a.x + size.cx + 8, a.y + 6};
        FillRectColor(dc, bg, RGB(13, 15, 18));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(248, 250, 252));
        TextOutW(dc, a.x, a.y - size.cy, op.text.c_str(), static_cast<int>(op.text.size()));
        SelectObject(dc, oldFont);
        DeleteObject(font);
    }
}

} // namespace ModernScreenshot
