#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <initguid.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

#ifndef VERSION
#define VERSION "0.1.0"
#endif

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)

using Gdiplus::Bitmap;
using Gdiplus::Color;
using Gdiplus::Font;
using Gdiplus::FontStyleBold;
using Gdiplus::FontStyleRegular;
using Gdiplus::Graphics;
using Gdiplus::ImageCodecInfo;
using Gdiplus::Pen;
using Gdiplus::PointF;
using Gdiplus::RectF;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
using Gdiplus::UnitPixel;

static const wchar_t *kVersion = WIDEN(VERSION);
static const wchar_t *kHiddenClass = L"ModernScreenshotHidden";
static const wchar_t *kOverlayClass = L"ModernScreenshotOverlay";
static const wchar_t *kEditorClass = L"ModernScreenshotEditor";
static const int kHotkeyId = 1001;
static const int kToolbarH = 52;
static const int kButtonW = 64;
static const int kButtonH = 36;
static const int kButtonGap = 8;

enum Tool {
    ToolBlur,
    ToolArrow,
    ToolRect,
    ToolText,
};

enum Action {
    ActionNone,
    ActionToolBlur,
    ActionToolArrow,
    ActionToolRect,
    ActionToolText,
    ActionUndo,
    ActionCopy,
    ActionSave,
};

struct ButtonSpec {
    Action action;
    Tool tool;
    const wchar_t *label;
};

struct Annotation {
    Tool tool;
    POINT a;
    POINT b;
    std::wstring text;
};

struct ScreenCapture {
    HBITMAP bitmap = NULL;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct OverlayState {
    ScreenCapture capture;
    bool dragging = false;
    bool done = false;
    bool cancelled = false;
    POINT start = {0, 0};
    POINT current = {0, 0};
    RECT selection = {0, 0, 0, 0};
};

struct EditorState {
    HBITMAP shot = NULL;
    int w = 0;
    int h = 0;
    int winW = 0;
    int winH = 0;
    int imageX = 0;
    int imageY = 0;
    Tool activeTool = ToolRect;
    std::vector<Annotation> ops;
    bool drawing = false;
    Annotation preview;
    bool textEntry = false;
    POINT textPoint = {0, 0};
    std::wstring textBuffer;
    bool done = false;
};

static ULONG_PTR g_gdiplusToken = 0;
static HINSTANCE g_instance = NULL;

static const ButtonSpec kButtons[] = {
    {ActionToolBlur, ToolBlur, L"Blur"},
    {ActionToolArrow, ToolArrow, L"->"},
    {ActionToolRect, ToolRect, L"Box"},
    {ActionToolText, ToolText, L"Text"},
    {ActionUndo, ToolBlur, L"Undo"},
    {ActionCopy, ToolBlur, L"Copy"},
    {ActionSave, ToolBlur, L"Save"},
};

static int ButtonCount() {
    return static_cast<int>(sizeof(kButtons) / sizeof(kButtons[0]));
}

static int ToolbarWidth() {
    return 24 + ButtonCount() * kButtonW + (ButtonCount() - 1) * kButtonGap;
}

static int Clamp(int value, int minValue, int maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static int RectWidth(const RECT &rect) {
    return rect.right - rect.left;
}

static int RectHeight(const RECT &rect) {
    return rect.bottom - rect.top;
}

static RECT NormalizeRectPoints(POINT a, POINT b) {
    RECT rect;
    rect.left = a.x < b.x ? a.x : b.x;
    rect.top = a.y < b.y ? a.y : b.y;
    rect.right = a.x > b.x ? a.x : b.x;
    rect.bottom = a.y > b.y ? a.y : b.y;
    return rect;
}

static RECT ClampRect(RECT rect, int left, int top, int right, int bottom) {
    rect.left = Clamp(rect.left, left, right);
    rect.right = Clamp(rect.right, left, right);
    rect.top = Clamp(rect.top, top, bottom);
    rect.bottom = Clamp(rect.bottom, top, bottom);
    if (rect.right < rect.left) {
        rect.right = rect.left;
    }
    if (rect.bottom < rect.top) {
        rect.bottom = rect.top;
    }
    return rect;
}

static RECT ButtonRect(int index) {
    RECT rect;
    rect.left = 12 + index * (kButtonW + kButtonGap);
    rect.top = 8;
    rect.right = rect.left + kButtonW;
    rect.bottom = rect.top + kButtonH;
    return rect;
}

static bool PointInRect(int x, int y, const RECT &rect) {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

static Action ButtonAt(int x, int y, Tool *tool) {
    for (int i = 0; i < ButtonCount(); ++i) {
        if (PointInRect(x, y, ButtonRect(i))) {
            *tool = kButtons[i].tool;
            return kButtons[i].action;
        }
    }
    return ActionNone;
}

static bool StartGdiplus() {
    if (g_gdiplusToken) {
        return true;
    }

    Gdiplus::GdiplusStartupInput input;
    return Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, NULL) == Gdiplus::Ok;
}

static void StopGdiplus() {
    if (g_gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

static bool GetEncoderClsid(const wchar_t *mimeType, CLSID *clsid) {
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

static bool SaveHbitmapPng(HBITMAP bitmap, const std::wstring &path) {
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

static std::wstring BuildScreenshotPath() {
    PWSTR pictures = NULL;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &pictures))) {
        dir.assign(pictures);
        CoTaskMemFree(pictures);
    } else {
        wchar_t fallback[MAX_PATH];
        GetTempPathW(MAX_PATH, fallback);
        dir.assign(fallback);
    }

    if (!dir.empty() && dir.back() != L'\\') {
        dir.push_back(L'\\');
    }
    dir += L"Screenshots";
    CreateDirectoryW(dir.c_str(), NULL);

    std::time_t now = std::time(NULL);
    std::tm localTime = {};
#ifdef _MSC_VER
    localtime_s(&localTime, &now);
#else
    std::tm *tmp = std::localtime(&now);
    if (tmp) {
        localTime = *tmp;
    }
#endif

    wchar_t stamp[32];
    wcsftime(stamp, 32, L"%Y%m%d-%H%M%S", &localTime);
    return dir + L"\\Screenshot-" + stamp + L".png";
}

static ScreenCapture CaptureVirtualScreen() {
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

static HBITMAP CropCapture(const ScreenCapture &capture, const RECT &selection) {
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

static void FillRectColor(HDC dc, const RECT &rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

static void DrawTextLabel(HDC dc, const std::wstring &text, int x, int y, COLORREF color) {
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

static void DrawOverlay(OverlayState *state, HWND hwnd, HDC dc) {
    HDC source = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(source, state->capture.bitmap);
    BitBlt(dc, 0, 0, state->capture.w, state->capture.h, source, 0, 0, SRCCOPY);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        SolidBrush shade(Color(120, 8, 12, 16));
        graphics.FillRectangle(&shade, 0, 0, state->capture.w, state->capture.h);
    }

    if (state->dragging) {
        RECT local = NormalizeRectPoints(state->start, state->current);
        local = ClampRect(local, 0, 0, state->capture.w, state->capture.h);
        if (RectWidth(local) > 0 && RectHeight(local) > 0) {
            BitBlt(dc, local.left, local.top, RectWidth(local), RectHeight(local), source,
                   local.left, local.top, SRCCOPY);
            if (StartGdiplus()) {
                Graphics graphics(dc);
                Pen accent(Color(255, 23, 198, 163), 2.0f);
                graphics.DrawRectangle(&accent, local.left, local.top, RectWidth(local), RectHeight(local));
            }

            wchar_t label[64];
            wsprintfW(label, L"%d x %d", RectWidth(local), RectHeight(local));
            RECT pill = {local.left, local.top - 28, local.left + 92, local.top - 4};
            if (pill.top < 8) {
                OffsetRect(&pill, 0, 36);
            }
            FillRectColor(dc, pill, RGB(16, 21, 24));
            DrawTextLabel(dc, label, pill.left + 10, pill.top + 4, RGB(245, 251, 248));
        }
    }

    SelectObject(source, old);
    DeleteDC(source);
    (void)hwnd;
}

static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    OverlayState *state = reinterpret_cast<OverlayState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        SetCursor(LoadCursor(NULL, IDC_CROSS));
        return 0;
    }
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_CROSS));
        return TRUE;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && state) {
            state->cancelled = true;
            state->done = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (state) {
            SetCapture(hwnd);
            state->dragging = true;
            state->start = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            state->current = state->start;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state && state->dragging) {
            state->current = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state && state->dragging) {
            ReleaseCapture();
            state->current = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT local = NormalizeRectPoints(state->start, state->current);
            local = ClampRect(local, 0, 0, state->capture.w, state->capture.h);
            if (RectWidth(local) >= 4 && RectHeight(local) >= 4) {
                state->selection = {local.left + state->capture.x, local.top + state->capture.y,
                                    local.right + state->capture.x, local.bottom + state->capture.y};
            } else {
                state->cancelled = true;
            }
            state->done = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            DrawOverlay(state, hwnd, dc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static bool RunSelectionOverlay(const ScreenCapture &capture, RECT *selection) {
    OverlayState state;
    state.capture = capture;

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kOverlayClass, L"Modern Screenshot",
                                WS_POPUP, capture.x, capture.y, capture.w, capture.h,
                                NULL, NULL, g_instance, &state);
    if (!hwnd) {
        return false;
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    MSG msg;
    while (!state.done && GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (state.cancelled || RectWidth(state.selection) <= 0 || RectHeight(state.selection) <= 0) {
        return false;
    }
    *selection = state.selection;
    return true;
}

static bool PointInImage(EditorState *state, int x, int y) {
    return x >= state->imageX && y >= state->imageY &&
           x < state->imageX + state->w && y < state->imageY + state->h;
}

static POINT ToImagePoint(EditorState *state, int x, int y) {
    POINT point;
    point.x = Clamp(x - state->imageX, 0, state->w - 1);
    point.y = Clamp(y - state->imageY, 0, state->h - 1);
    return point;
}

static void DrawToolbar(EditorState *state, HDC dc) {
    RECT toolbar = {0, 0, state->winW, kToolbarH};
    FillRectColor(dc, toolbar, RGB(16, 20, 24));

    HPEN border = CreatePen(PS_SOLID, 1, RGB(58, 74, 80));
    HGDIOBJ oldPen = SelectObject(dc, border);
    MoveToEx(dc, 0, kToolbarH - 1, NULL);
    LineTo(dc, state->winW, kToolbarH - 1);
    SelectObject(dc, oldPen);
    DeleteObject(border);

    for (int i = 0; i < ButtonCount(); ++i) {
        RECT button = ButtonRect(i);
        bool isTool = kButtons[i].action == ActionToolBlur || kButtons[i].action == ActionToolArrow ||
                      kButtons[i].action == ActionToolRect || kButtons[i].action == ActionToolText;
        bool active = isTool && kButtons[i].tool == state->activeTool;
        FillRectColor(dc, button, active ? RGB(33, 49, 56) : RGB(22, 29, 34));
        HPEN pen = CreatePen(PS_SOLID, 1, active ? RGB(23, 198, 163) : RGB(58, 74, 80));
        HGDIOBJ old = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, button.left, button.top, button.right, button.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, old);
        DeleteObject(pen);

        SIZE size;
        HFONT font = CreateFontW(-14, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(dc, font);
        GetTextExtentPoint32W(dc, kButtons[i].label, static_cast<int>(wcslen(kButtons[i].label)), &size);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, active ? RGB(23, 198, 163) : RGB(245, 251, 248));
        TextOutW(dc, button.left + (kButtonW - size.cx) / 2, button.top + 10,
                 kButtons[i].label, static_cast<int>(wcslen(kButtons[i].label)));
        SelectObject(dc, oldFont);
        DeleteObject(font);
    }
}

static void ApplyMosaic(HDC dc, RECT rect) {
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

static void DrawArrow(HDC dc, POINT a, POINT b, COLORREF color) {
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

static void DrawAnnotation(HDC dc, const Annotation &op, int offsetX, int offsetY, int maxW, int maxH) {
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
        HPEN pen = CreatePen(PS_SOLID, 3, RGB(23, 198, 163));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    } else if (op.tool == ToolArrow) {
        DrawArrow(dc, a, b, RGB(255, 207, 90));
    } else if (op.tool == ToolText) {
        SIZE size;
        HFONT font = CreateFontW(-18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HGDIOBJ oldFont = SelectObject(dc, font);
        GetTextExtentPoint32W(dc, op.text.c_str(), static_cast<int>(op.text.size()), &size);
        RECT bg = {a.x - 6, a.y - size.cy - 6, a.x + size.cx + 8, a.y + 6};
        FillRectColor(dc, bg, RGB(16, 20, 24));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 207, 90));
        TextOutW(dc, a.x, a.y - size.cy, op.text.c_str(), static_cast<int>(op.text.size()));
        SelectObject(dc, oldFont);
        DeleteObject(font);
    }
}

static HBITMAP ComposeEditorBitmap(EditorState *state) {
    HDC screen = GetDC(NULL);
    HDC source = CreateCompatibleDC(screen);
    HDC dest = CreateCompatibleDC(screen);
    HBITMAP output = CreateCompatibleBitmap(screen, state->w, state->h);
    HGDIOBJ oldSource = SelectObject(source, state->shot);
    HGDIOBJ oldDest = SelectObject(dest, output);
    BitBlt(dest, 0, 0, state->w, state->h, source, 0, 0, SRCCOPY);

    for (const Annotation &op : state->ops) {
        DrawAnnotation(dest, op, 0, 0, state->w, state->h);
    }

    SelectObject(source, oldSource);
    SelectObject(dest, oldDest);
    DeleteDC(source);
    DeleteDC(dest);
    ReleaseDC(NULL, screen);
    return output;
}

static bool SaveEditor(EditorState *state) {
    HBITMAP output = ComposeEditorBitmap(state);
    if (!output) {
        return false;
    }
    bool ok = SaveHbitmapPng(output, BuildScreenshotPath());
    DeleteObject(output);
    return ok;
}

static bool CopyEditor(EditorState *state, HWND hwnd) {
    HBITMAP output = ComposeEditorBitmap(state);
    if (!output) {
        return false;
    }
    if (!OpenClipboard(hwnd)) {
        DeleteObject(output);
        return false;
    }
    EmptyClipboard();
    if (!SetClipboardData(CF_BITMAP, output)) {
        CloseClipboard();
        DeleteObject(output);
        return false;
    }
    CloseClipboard();
    return true;
}

static void DrawEditor(EditorState *state, HWND hwnd, HDC dc) {
    RECT bg = {0, 0, state->winW, state->winH};
    FillRectColor(dc, bg, RGB(16, 20, 24));

    HDC source = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(source, state->shot);
    BitBlt(dc, state->imageX, state->imageY, state->w, state->h, source, 0, 0, SRCCOPY);
    SelectObject(source, old);
    DeleteDC(source);

    RECT imageBorder = {state->imageX, state->imageY, state->imageX + state->w, state->imageY + state->h};
    HPEN border = CreatePen(PS_SOLID, 1, RGB(58, 74, 80));
    HGDIOBJ oldPen = SelectObject(dc, border);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, imageBorder.left, imageBorder.top, imageBorder.right, imageBorder.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(border);

    for (const Annotation &op : state->ops) {
        DrawAnnotation(dc, op, state->imageX, state->imageY, state->w, state->h);
    }
    if (state->drawing) {
        DrawAnnotation(dc, state->preview, state->imageX, state->imageY, state->w, state->h);
    }
    if (state->textEntry) {
        Annotation text;
        text.tool = ToolText;
        text.a = state->textPoint;
        text.b = state->textPoint;
        text.text = state->textBuffer + L"_";
        DrawAnnotation(dc, text, state->imageX, state->imageY, state->w, state->h);
    }

    DrawToolbar(state, dc);
    (void)hwnd;
}

static void CommitText(EditorState *state) {
    if (state->textBuffer.empty()) {
        state->textEntry = false;
        return;
    }

    Annotation text;
    text.tool = ToolText;
    text.a = state->textPoint;
    text.b = state->textPoint;
    text.text = state->textBuffer;
    state->ops.push_back(text);
    state->textBuffer.clear();
    state->textEntry = false;
}

static void FinishDrawing(EditorState *state, POINT point) {
    state->preview.b = point;
    RECT rect = NormalizeRectPoints(state->preview.a, state->preview.b);
    bool keep = false;
    if (state->preview.tool == ToolArrow) {
        int dx = state->preview.b.x - state->preview.a.x;
        int dy = state->preview.b.y - state->preview.a.y;
        keep = dx * dx + dy * dy > 25;
    } else {
        keep = RectWidth(rect) >= 4 && RectHeight(rect) >= 4;
    }

    if (keep) {
        state->ops.push_back(state->preview);
    }
    state->drawing = false;
}

static void HandleAction(EditorState *state, HWND hwnd, Action action, Tool tool) {
    if (action == ActionToolBlur || action == ActionToolArrow || action == ActionToolRect || action == ActionToolText) {
        state->activeTool = tool;
        state->textEntry = false;
    } else if (action == ActionUndo) {
        if (!state->ops.empty()) {
            state->ops.pop_back();
        }
        state->textEntry = false;
    } else if (action == ActionSave) {
        SaveEditor(state);
        state->done = true;
        DestroyWindow(hwnd);
    } else if (action == ActionCopy) {
        CopyEditor(state, hwnd);
        state->done = true;
        DestroyWindow(hwnd);
    }
}

static LRESULT CALLBACK EditorProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    EditorState *state = reinterpret_cast<EditorState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        SetFocus(hwnd);
        return 0;
    }
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    case WM_KEYDOWN:
        if (!state) {
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'Z') {
            if (!state->ops.empty()) {
                state->ops.pop_back();
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'S') {
            HandleAction(state, hwnd, ActionSave, ToolBlur);
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'C') {
            HandleAction(state, hwnd, ActionCopy, ToolBlur);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (state->textEntry) {
            if (wParam == VK_RETURN) {
                CommitText(state);
            } else if (wParam == VK_BACK && !state->textBuffer.empty()) {
                state->textBuffer.pop_back();
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (wParam == 'B') {
            state->activeTool = ToolBlur;
        } else if (wParam == 'A') {
            state->activeTool = ToolArrow;
        } else if (wParam == 'R') {
            state->activeTool = ToolRect;
        } else if (wParam == 'T') {
            state->activeTool = ToolText;
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_CHAR:
        if (state && state->textEntry && wParam >= 32 && wParam != 127 && state->textBuffer.size() < 120) {
            state->textBuffer.push_back(static_cast<wchar_t>(wParam));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (!state) {
            return 0;
        }
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            Tool tool = ToolBlur;
            Action action = ButtonAt(x, y, &tool);
            if (action != ActionNone) {
                HandleAction(state, hwnd, action, tool);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (!PointInImage(state, x, y)) {
                return 0;
            }

            POINT point = ToImagePoint(state, x, y);
            if (state->activeTool == ToolText) {
                state->textEntry = true;
                state->textPoint = point;
                state->textBuffer.clear();
            } else {
                SetCapture(hwnd);
                state->drawing = true;
                state->preview.tool = state->activeTool;
                state->preview.a = point;
                state->preview.b = point;
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state && state->drawing) {
            POINT point = ToImagePoint(state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            state->preview.b = point;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state && state->drawing) {
            ReleaseCapture();
            POINT point = ToImagePoint(state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            FinishDrawing(state, point);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            DrawEditor(state, hwnd, dc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        if (state) {
            state->done = true;
        }
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static void RunEditorModal(HBITMAP shot, int w, int h) {
    EditorState state;
    state.shot = shot;
    state.w = w;
    state.h = h;
    state.winW = std::max(w, ToolbarWidth());
    state.winH = h + kToolbarH;
    state.imageX = (state.winW - w) / 2;
    state.imageY = kToolbarH;

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int x = vx + std::max(0, (vw - state.winW) / 2);
    int y = vy + std::max(0, (vh - state.winH) / 2);

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kEditorClass, L"ModernScreenshot",
                                WS_POPUP, x, y, state.winW, state.winH,
                                NULL, NULL, g_instance, &state);
    if (!hwnd) {
        return;
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    MSG msg;
    while (!state.done && GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

static void RunCapture() {
    ScreenCapture capture = CaptureVirtualScreen();
    if (!capture.bitmap) {
        return;
    }

    RECT selection;
    if (RunSelectionOverlay(capture, &selection)) {
        HBITMAP shot = CropCapture(capture, selection);
        if (shot) {
            RunEditorModal(shot, RectWidth(selection), RectHeight(selection));
            DeleteObject(shot);
        }
    }
    DeleteObject(capture.bitmap);
}

static bool SelfTestExport(const std::wstring &path) {
    HDC screen = GetDC(NULL);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 360, 220);
    HGDIOBJ old = SelectObject(dc, bitmap);
    RECT bg = {0, 0, 360, 220};
    FillRectColor(dc, bg, RGB(22, 29, 34));
    for (int x = 0; x < 360; x += 24) {
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(58, 74, 80));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        MoveToEx(dc, x, 0, NULL);
        LineTo(dc, x, 220);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
    for (int y = 0; y < 220; y += 24) {
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(58, 74, 80));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        MoveToEx(dc, 0, y, NULL);
        LineTo(dc, 360, y);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
    SelectObject(dc, old);
    DeleteDC(dc);
    ReleaseDC(NULL, screen);

    EditorState state;
    state.shot = bitmap;
    state.w = 360;
    state.h = 220;
    state.ops.push_back({ToolBlur, {24, 24}, {156, 92}, L""});
    state.ops.push_back({ToolArrow, {48, 174}, {204, 128}, L""});
    state.ops.push_back({ToolRect, {202, 42}, {328, 148}, L""});
    state.ops.push_back({ToolText, {210, 192}, {210, 192}, L"Modern"});

    HBITMAP output = ComposeEditorBitmap(&state);
    bool ok = output && SaveHbitmapPng(output, path);
    if (output) {
        DeleteObject(output);
    }
    DeleteObject(bitmap);
    return ok;
}

static LRESULT CALLBACK HiddenProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_HOTKEY:
        if (wParam == kHotkeyId) {
            RunCapture();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static void RegisterWindowClasses() {
    WNDCLASSW hidden = {};
    hidden.lpfnWndProc = HiddenProc;
    hidden.hInstance = g_instance;
    hidden.lpszClassName = kHiddenClass;
    RegisterClassW(&hidden);

    WNDCLASSW overlay = {};
    overlay.lpfnWndProc = OverlayProc;
    overlay.hInstance = g_instance;
    overlay.hCursor = LoadCursor(NULL, IDC_CROSS);
    overlay.lpszClassName = kOverlayClass;
    RegisterClassW(&overlay);

    WNDCLASSW editor = {};
    editor.lpfnWndProc = EditorProc;
    editor.hInstance = g_instance;
    editor.hCursor = LoadCursor(NULL, IDC_ARROW);
    editor.lpszClassName = kEditorClass;
    RegisterClassW(&editor);
}

static int RunDaemon() {
    HWND hwnd = CreateWindowExW(0, kHiddenClass, L"ModernScreenshot", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, g_instance, NULL);
    if (!hwnd) {
        return 1;
    }

    if (!RegisterHotKey(hwnd, kHotkeyId, MOD_CONTROL | MOD_NOREPEAT, '1')) {
        MessageBoxW(NULL, L"Ctrl+1 is already in use.", L"ModernScreenshot", MB_ICONERROR);
        DestroyWindow(hwnd);
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterHotKey(hwnd, kHotkeyId);
    DestroyWindow(hwnd);
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    g_instance = instance;
    SetProcessDPIAware();
    StartGdiplus();
    RegisterWindowClasses();

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int status = 0;
    if (argc > 1 && wcscmp(argv[1], L"--version") == 0) {
        status = 0;
    } else if (argc > 2 && wcscmp(argv[1], L"--self-test") == 0) {
        status = SelfTestExport(argv[2]) ? 0 : 1;
    } else if (argc > 1 && wcscmp(argv[1], L"--once") == 0) {
        RunCapture();
        status = 0;
    } else if (argc == 1) {
        status = RunDaemon();
    } else {
        MessageBoxW(NULL,
                    L"Usage:\nModernScreenshot.exe\nModernScreenshot.exe --once\nModernScreenshot.exe --self-test PATH",
                    L"ModernScreenshot", MB_OK);
        status = 2;
    }

    if (argv) {
        LocalFree(argv);
    }
    StopGdiplus();
    (void)kVersion;
    return status;
}
