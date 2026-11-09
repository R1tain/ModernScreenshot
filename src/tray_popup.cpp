#include "tray_popup.h"

namespace ModernScreenshot {

static constexpr int kTrayPopupRowH = 38;
static constexpr int kTrayPopupSepH = 10;
static constexpr int kTrayPopupHeaderH = 36;
static constexpr int kTrayPopupPadX = 14;
static constexpr int kTrayPopupRadius = 10;

static HWND g_trayPopup = NULL;
static HWND g_popupHiddenWnd = NULL;
static Settings g_popupSettings;

static int TrayPopupHitIndex(const TrayPopupState &state, int x, int y) {
    if (y < kTrayPopupHeaderH) return -1;
    int cy = kTrayPopupHeaderH;
    for (int i = 0; i < state.itemCount; ++i) {
        int h = state.items[i].isSeparator ? kTrayPopupSepH : kTrayPopupRowH;
        if (y >= cy && y < cy + h) {
            return state.items[i].isSeparator ? -1 : i;
        }
        cy += h;
    }
    (void)x;
    return -1;
}

static void PopulateTrayPopupItems(TrayPopupState *state) {
    static const wchar_t *captureGlyph = L"▢";
    static const wchar_t *longGlyph = L"≡";
    static const wchar_t *settingsGlyph = L"⚙";
    static const wchar_t *exitGlyph = L"✕";

    state->itemCount = 0;
    state->items[state->itemCount++] = {kMenuCapture, L"Capture", L"", captureGlyph, false};
    state->items[state->itemCount++] = {kMenuLongCapture, L"Long capture", L"", longGlyph, false};
    state->items[state->itemCount++] = {0, NULL, NULL, NULL, true};
    state->items[state->itemCount++] = {kMenuSettings, L"Settings", L"", settingsGlyph, false};
    state->items[state->itemCount++] = {kMenuExit, L"Exit", L"", exitGlyph, false};
}

static int TrayPopupBodyHeight(const TrayPopupState &state) {
    int h = kTrayPopupHeaderH;
    for (int i = 0; i < state.itemCount; ++i) {
        h += state.items[i].isSeparator ? kTrayPopupSepH : kTrayPopupRowH;
    }
    return h + 8;
}

static void DrawTrayPopupBackground(Graphics *graphics, const TrayPopupState &state) {
    GraphicsPath path;
    BuildRoundedRectPath(&path, 0.0f, 0.0f,
                         static_cast<float>(state.width), static_cast<float>(state.height),
                         static_cast<float>(kTrayPopupRadius));
    SolidBrush fill(Color(255, 13, 15, 18));
    Pen border(Color(255, 38, 44, 58), 1.0f);
    graphics->FillPath(&fill, &path);
    graphics->DrawPath(&border, &path);
}

static void DrawTrayPopupHeader(Graphics *graphics, const TrayPopupState &state) {
    Font title(L"Segoe UI", 11.0f, FontStyleBold, UnitPixel);
    SolidBrush titleBrush(Color(255, 248, 250, 252));
    graphics->DrawString(L"ModernScreenshot", -1, &title,
                         PointF(static_cast<float>(kTrayPopupPadX), 10.0f), &titleBrush);

    Font ver(L"Segoe UI", 10.0f, FontStyleBold, UnitPixel);
    SolidBrush verBrush(Color(255, 129, 140, 248));
    std::wstring vtext = L"v";
    vtext += kVersion;
    RectF vRect(static_cast<float>(state.width - kTrayPopupPadX - 60), 10.0f, 60.0f, 16.0f);
    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(Gdiplus::StringAlignmentFar);
    graphics->DrawString(vtext.c_str(), -1, &ver, vRect, &fmt, &verBrush);

    Pen rule(Color(255, 32, 38, 50), 1.0f);
    graphics->DrawLine(&rule, 12, kTrayPopupHeaderH - 1,
                       state.width - 12, kTrayPopupHeaderH - 1);
}

static void DrawTrayPopupRow(Graphics *graphics, const TrayPopupItem &item, const RECT &rect, bool hover) {
    if (item.isSeparator) {
        Pen line(Color(255, 32, 38, 50), 1.0f);
        INT midY = static_cast<INT>((rect.top + rect.bottom) / 2);
        graphics->DrawLine(&line, static_cast<INT>(rect.left + 8), midY,
                           static_cast<INT>(rect.right - 8), midY);
        return;
    }

    if (hover) {
        GraphicsPath path;
        BuildRoundedRectPath(&path, static_cast<float>(rect.left), static_cast<float>(rect.top + 2),
                             static_cast<float>(RectWidth(rect)),
                             static_cast<float>(RectHeight(rect) - 4), 6.0f);
        SolidBrush fill(Color(255, 28, 33, 48));
        graphics->FillPath(&fill, &path);
    }

    float midY = (rect.top + rect.bottom) / 2.0f;

    Font glyph(L"Segoe UI", 15.0f, FontStyleRegular, UnitPixel);
    SolidBrush glyphBrush(hover ? Color(255, 129, 140, 248) : Color(255, 148, 163, 184));
    graphics->DrawString(item.glyph, -1, &glyph,
                         PointF(static_cast<float>(rect.left + kTrayPopupPadX), midY - 11.0f),
                         &glyphBrush);

    Font label(L"Segoe UI", 12.0f, FontStyleRegular, UnitPixel);
    SolidBrush labelBrush(Color(255, 248, 250, 252));
    graphics->DrawString(item.label, -1, &label,
                         PointF(static_cast<float>(rect.left + kTrayPopupPadX + 28), midY - 9.0f),
                         &labelBrush);

    if (item.command == kMenuCapture) {
        std::wstring keys = HotkeyText(g_popupSettings);
        Font ks(L"Segoe UI", 11.0f, FontStyleRegular, UnitPixel);
        SolidBrush ksBrush(Color(255, 148, 163, 184));
        RectF ksRect(static_cast<float>(rect.right - 120), midY - 8.0f, 110.0f, 16.0f);
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(Gdiplus::StringAlignmentFar);
        graphics->DrawString(keys.c_str(), -1, &ks, ksRect, &fmt, &ksBrush);
    }
}

static void PaintTrayPopup(HWND hwnd, HDC dc, const TrayPopupState &state) {
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP buffer = CreateCompatibleBitmap(dc, state.width, state.height);
    HGDIOBJ oldBuf = SelectObject(mem, buffer);

    RECT full = {0, 0, state.width, state.height};
    HBRUSH bg = CreateSolidBrush(RGB(13, 15, 18));
    FillRect(mem, &full, bg);
    DeleteObject(bg);

    if (StartGdiplus()) {
        Graphics graphics(mem);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        DrawTrayPopupBackground(&graphics, state);
        DrawTrayPopupHeader(&graphics, state);

        int cy = kTrayPopupHeaderH;
        for (int i = 0; i < state.itemCount; ++i) {
            int h = state.items[i].isSeparator ? kTrayPopupSepH : kTrayPopupRowH;
            RECT row = {4, cy, state.width - 4, cy + h};
            DrawTrayPopupRow(&graphics, state.items[i], row, i == state.hoverIndex);
            cy += h;
        }
    }

    BitBlt(dc, 0, 0, state.width, state.height, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBuf);
    DeleteObject(buffer);
    DeleteDC(mem);
    (void)hwnd;
}

static void TrayPopupExecute(int index, const TrayPopupState &state) {
    if (index < 0 || index >= state.itemCount) return;
    const TrayPopupItem &item = state.items[index];
    if (item.isSeparator || item.command == 0) return;
    if (g_popupHiddenWnd) {
        PostMessageW(g_popupHiddenWnd, WM_COMMAND, MAKEWPARAM(item.command, 0), 0);
    }
}

static int FirstSelectableIndex(const TrayPopupState &state, int start, int direction) {
    if (state.itemCount == 0) return -1;
    int i = start;
    for (int n = 0; n < state.itemCount; ++n) {
        if (i < 0) i = state.itemCount - 1;
        if (i >= state.itemCount) i = 0;
        if (!state.items[i].isSeparator) return i;
        i += direction;
    }
    return -1;
}

LRESULT CALLBACK TrayPopupProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    TrayPopupState *state = reinterpret_cast<TrayPopupState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        TrayPopupState *view = new TrayPopupState();
        PopulateTrayPopupItems(view);
        view->width = create->cx;
        view->height = create->cy;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        HRGN region = CreateRoundRectRgn(0, 0, view->width + 1, view->height + 1,
                                         kTrayPopupRadius * 2, kTrayPopupRadius * 2);
        SetWindowRgn(hwnd, region, FALSE);
        return 0;
    }
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_MOUSEMOVE: {
        if (!state) return 0;
        int hit = TrayPopupHitIndex(*state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        if (hit != state->hoverIndex) {
            state->hoverIndex = hit;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&track);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (state && state->hoverIndex != -1) {
            state->hoverIndex = -1;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP: {
        if (!state) return 0;
        int hit = TrayPopupHitIndex(*state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        TrayPopupState copy = *state;
        state->dismissing = true;
        DestroyWindow(hwnd);
        TrayPopupExecute(hit, copy);
        return 0;
    }
    case WM_RBUTTONUP:
        if (state) {
            state->dismissing = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_KEYDOWN: {
        if (!state) return 0;
        if (wParam == VK_ESCAPE) {
            state->dismissing = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == VK_DOWN || wParam == VK_TAB) {
            int start = (state->hoverIndex < 0) ? 0 : state->hoverIndex + 1;
            state->hoverIndex = FirstSelectableIndex(*state, start, 1);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (wParam == VK_UP) {
            int start = (state->hoverIndex < 0) ? state->itemCount - 1 : state->hoverIndex - 1;
            state->hoverIndex = FirstSelectableIndex(*state, start, -1);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (wParam == VK_RETURN || wParam == VK_SPACE) {
            TrayPopupState copy = *state;
            int hover = state->hoverIndex;
            state->dismissing = true;
            DestroyWindow(hwnd);
            TrayPopupExecute(hover, copy);
            return 0;
        }
        return 0;
    }
    case WM_KILLFOCUS:
    case WM_ACTIVATEAPP:
        if (state && !state->dismissing) {
            state->dismissing = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            PaintTrayPopup(hwnd, dc, *state);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCDESTROY:
        if (state) {
            delete state;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        if (g_trayPopup == hwnd) {
            g_trayPopup = NULL;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ShowTrayPopup(HWND hwnd, HINSTANCE instance, HWND hiddenWnd, const Settings &settings) {
    if (g_trayPopup) {
        DestroyWindow(g_trayPopup);
        g_trayPopup = NULL;
    }

    g_popupHiddenWnd = hiddenWnd;
    g_popupSettings = settings;

    TrayPopupState measure;
    PopulateTrayPopupItems(&measure);
    int width = 260;
    int height = TrayPopupBodyHeight(measure);
    measure.width = width;
    measure.height = height;

    POINT cursor;
    GetCursorPos(&cursor);
    HMONITOR mon = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);

    int x = cursor.x - width;
    int y = cursor.y - height;
    if (x < mi.rcWork.left + 4) x = mi.rcWork.left + 4;
    if (y < mi.rcWork.top + 4) y = mi.rcWork.top + 4;
    if (x + width > mi.rcWork.right - 4) x = mi.rcWork.right - 4 - width;
    if (y + height > mi.rcWork.bottom - 4) y = mi.rcWork.bottom - 4 - height;

    g_trayPopup = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                                  kTrayPopupClass, L"", WS_POPUP, x, y, width, height,
                                  hwnd, NULL, instance, NULL);
    if (!g_trayPopup) return;

    SetForegroundWindow(hwnd);
    ShowWindow(g_trayPopup, SW_SHOWNOACTIVATE);
    SetForegroundWindow(g_trayPopup);
    SetFocus(g_trayPopup);
}

} // namespace ModernScreenshot
