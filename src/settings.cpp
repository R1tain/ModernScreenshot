#include "settings.h"
#include "app.h"

namespace ModernScreenshot {

static const int kSettingsClientW = 480;
static const int kSettingsClientH = 420;
static const int kSettingsPadX = 28;

static HWND g_settingsWindow = NULL;

static RECT SettingsHotkeyRect() {
    RECT r = {kSettingsPadX, 92, kSettingsClientW - kSettingsPadX, 162};
    return r;
}

static RECT SettingsStartupRect() {
    RECT r = {kSettingsPadX, 232, kSettingsClientW - kSettingsPadX, 286};
    return r;
}

static RECT SettingsBannerRect() {
    RECT r = {kSettingsPadX, 300, kSettingsClientW - kSettingsPadX, 336};
    return r;
}

static RECT SettingsApplyRect() {
    RECT r = {kSettingsClientW - kSettingsPadX - 120, kSettingsClientH - 60,
              kSettingsClientW - kSettingsPadX, kSettingsClientH - 24};
    return r;
}

static RECT SettingsCancelRect() {
    RECT r = {kSettingsClientW - kSettingsPadX - 220, kSettingsClientH - 60,
              kSettingsClientW - kSettingsPadX - 130, kSettingsClientH - 24};
    return r;
}

static SettingsHit SettingsHitTest(int x, int y) {
    POINT p = {x, y};
    RECT r;
    r = SettingsHotkeyRect();
    if (PtInRect(&r, p)) return SettingsHitHotkey;
    r = SettingsStartupRect();
    if (PtInRect(&r, p)) return SettingsHitStartup;
    r = SettingsApplyRect();
    if (PtInRect(&r, p)) return SettingsHitApply;
    r = SettingsCancelRect();
    if (PtInRect(&r, p)) return SettingsHitCancel;
    return SettingsHitNone;
}

static void DrawSettingsTitle(Graphics *graphics) {
    Font font(L"Segoe UI", 18.0f, FontStyleBold, UnitPixel);
    SolidBrush brush(Color(255, 245, 251, 248));
    graphics->DrawString(L"Settings", -1, &font, PointF(kSettingsPadX, 28), &brush);

    Font sub(L"Segoe UI", 11.0f, FontStyleRegular, UnitPixel);
    SolidBrush subBrush(Color(220, 138, 156, 158));
    std::wstring version = L"ModernScreenshot ";
    version += kVersion;
    graphics->DrawString(version.c_str(), -1, &sub, PointF(kSettingsPadX, 56), &subBrush);
}

static void DrawSettingsSectionHeader(Graphics *graphics, const wchar_t *text, float y) {
    Font font(L"Segoe UI", 10.0f, FontStyleBold, UnitPixel);
    SolidBrush brush(Color(255, 116, 140, 142));
    graphics->DrawString(text, -1, &font, PointF(kSettingsPadX, y), &brush);
}

static void DrawSettingsHotkeyCard(Graphics *graphics, const RECT &rect, const SettingsViewState &state) {
    GraphicsPath path;
    BuildRoundedRectPath(&path, static_cast<float>(rect.left), static_cast<float>(rect.top),
                         static_cast<float>(RectWidth(rect)), static_cast<float>(RectHeight(rect)), 10.0f);
    bool hover = state.hover == SettingsHitHotkey || state.capturing;
    SolidBrush fill(state.capturing ? Color(255, 23, 56, 50) :
                    hover ? Color(255, 36, 46, 52) : Color(255, 28, 36, 42));
    Pen border(state.capturing ? Color(255, 23, 198, 163) :
               hover ? Color(255, 64, 84, 88) : Color(255, 50, 64, 70),
               state.capturing ? 1.8f : 1.0f);
    graphics->FillPath(&fill, &path);
    graphics->DrawPath(&border, &path);

    Font hintFont(L"Segoe UI", 10.0f, FontStyleRegular, UnitPixel);
    SolidBrush hintBrush(Color(255, 138, 156, 158));
    graphics->DrawString(state.capturing ? L"Press a key combination" : L"Capture hotkey",
                         -1, &hintFont, PointF(rect.left + 16.0f, rect.top + 10.0f), &hintBrush);

    std::wstring label;
    if (state.capturing) {
        label = L"Listening...";
    } else {
        label = HotkeyText(state.pending);
    }

    Font keyFont(L"Segoe UI", 16.0f, FontStyleBold, UnitPixel);
    SolidBrush keyBrush(state.capturing ? Color(255, 23, 198, 163) : Color(255, 245, 251, 248));
    graphics->DrawString(label.c_str(), -1, &keyFont, PointF(rect.left + 16.0f, rect.top + 32.0f), &keyBrush);

    if (!state.capturing) {
        Font edit(L"Segoe UI", 12.0f, FontStyleRegular, UnitPixel);
        SolidBrush editBrush(Color(255, 138, 156, 158));
        RectF editRect(static_cast<float>(rect.right - 90), static_cast<float>(rect.top + 38), 80.0f, 20.0f);
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(Gdiplus::StringAlignmentFar);
        graphics->DrawString(L"Click to change", -1, &edit, editRect, &fmt, &editBrush);
    }
}

static void DrawSettingsCheckRow(Graphics *graphics, const RECT &rect, const SettingsViewState &state) {
    bool hover = state.hover == SettingsHitStartup;
    if (hover) {
        GraphicsPath path;
        BuildRoundedRectPath(&path, static_cast<float>(rect.left - 8), static_cast<float>(rect.top - 6),
                             static_cast<float>(RectWidth(rect) + 16), static_cast<float>(RectHeight(rect) + 12), 8.0f);
        SolidBrush fill(Color(255, 30, 38, 44));
        graphics->FillPath(&fill, &path);
    }

    float bx = static_cast<float>(rect.left);
    float by = static_cast<float>(rect.top);
    GraphicsPath box;
    BuildRoundedRectPath(&box, bx, by + 2.0f, 20.0f, 20.0f, 5.0f);
    SolidBrush boxFill(state.startup ? Color(255, 23, 198, 163) : Color(255, 24, 32, 38));
    Pen boxPen(state.startup ? Color(255, 23, 198, 163) : Color(255, 90, 108, 112), 1.4f);
    graphics->FillPath(&boxFill, &box);
    graphics->DrawPath(&boxPen, &box);
    if (state.startup) {
        Pen check(Color(255, 16, 22, 26), 2.2f);
        graphics->DrawLine(&check, bx + 5.0f, by + 12.0f, bx + 9.0f, by + 16.0f);
        graphics->DrawLine(&check, bx + 9.0f, by + 16.0f, bx + 16.0f, by + 7.0f);
    }

    Font label(L"Segoe UI", 13.0f, FontStyleRegular, UnitPixel);
    SolidBrush labelBrush(Color(255, 240, 246, 244));
    graphics->DrawString(L"Start with Windows", -1, &label, PointF(bx + 32.0f, by - 1.0f), &labelBrush);

    Font desc(L"Segoe UI", 11.0f, FontStyleRegular, UnitPixel);
    SolidBrush descBrush(Color(255, 138, 156, 158));
    graphics->DrawString(L"Launch ModernScreenshot when you sign in",
                         -1, &desc, PointF(bx + 32.0f, by + 22.0f), &descBrush);
}

static void DrawSettingsBanner(Graphics *graphics, const RECT &rect, const SettingsViewState &state) {
    if (state.banner == BannerNone || state.status.empty()) {
        return;
    }
    Color fillColor, borderColor, textColor, glyphColor;
    const wchar_t *glyph = L"i";
    if (state.banner == BannerSuccess) {
        fillColor = Color(255, 19, 60, 50);
        borderColor = Color(255, 23, 198, 163);
        textColor = Color(255, 230, 252, 244);
        glyphColor = Color(255, 23, 198, 163);
        glyph = L"✓";
    } else if (state.banner == BannerWarning) {
        fillColor = Color(255, 70, 52, 24);
        borderColor = Color(255, 255, 184, 60);
        textColor = Color(255, 254, 232, 196);
        glyphColor = Color(255, 255, 184, 60);
        glyph = L"!";
    } else {
        fillColor = Color(255, 32, 42, 50);
        borderColor = Color(255, 86, 110, 124);
        textColor = Color(255, 220, 232, 234);
        glyphColor = Color(255, 138, 178, 198);
    }

    GraphicsPath path;
    BuildRoundedRectPath(&path, static_cast<float>(rect.left), static_cast<float>(rect.top),
                         static_cast<float>(RectWidth(rect)), static_cast<float>(RectHeight(rect)), 8.0f);
    SolidBrush fill(fillColor);
    Pen border(borderColor, 1.2f);
    graphics->FillPath(&fill, &path);
    graphics->DrawPath(&border, &path);

    Font glyphFont(L"Segoe UI", 14.0f, FontStyleBold, UnitPixel);
    SolidBrush glyphBrush(glyphColor);
    graphics->DrawString(glyph, -1, &glyphFont, PointF(rect.left + 14.0f, rect.top + 7.0f), &glyphBrush);

    Font textFont(L"Segoe UI", 12.0f, FontStyleRegular, UnitPixel);
    SolidBrush textBrush(textColor);
    graphics->DrawString(state.status.c_str(), -1, &textFont,
                         PointF(rect.left + 36.0f, rect.top + 9.0f), &textBrush);
}

static void DrawSettingsButton(Graphics *graphics, const RECT &rect, const wchar_t *label,
                               bool primary, bool hovered, bool pressed) {
    GraphicsPath path;
    BuildRoundedRectPath(&path, static_cast<float>(rect.left), static_cast<float>(rect.top),
                         static_cast<float>(RectWidth(rect)), static_cast<float>(RectHeight(rect)), 9.0f);
    Color fillColor, borderColor, textColor;
    if (primary) {
        BYTE alpha = pressed ? 220 : (hovered ? 255 : 240);
        fillColor = Color(alpha, 23, 198, 163);
        borderColor = Color(255, 23, 198, 163);
        textColor = Color(255, 8, 22, 18);
    } else {
        fillColor = pressed ? Color(255, 36, 46, 52) :
                    hovered ? Color(255, 32, 42, 48) : Color(0, 0, 0, 0);
        borderColor = hovered ? Color(255, 138, 156, 158) : Color(255, 86, 110, 114);
        textColor = Color(255, 230, 240, 240);
    }
    SolidBrush fill(fillColor);
    Pen border(borderColor, 1.1f);
    if (fillColor.GetA() > 0) {
        graphics->FillPath(&fill, &path);
    }
    graphics->DrawPath(&border, &path);

    Font font(L"Segoe UI", 13.0f, primary ? FontStyleBold : FontStyleRegular, UnitPixel);
    SolidBrush textBrush(textColor);
    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    RectF text(static_cast<float>(rect.left), static_cast<float>(rect.top),
               static_cast<float>(RectWidth(rect)), static_cast<float>(RectHeight(rect)));
    graphics->DrawString(label, -1, &font, text, &fmt, &textBrush);
}

static void DrawSettingsHelpHint(Graphics *graphics, float y) {
    Font font(L"Segoe UI", 10.0f, FontStyleRegular, UnitPixel);
    SolidBrush brush(Color(255, 96, 116, 120));
    graphics->DrawString(L"Esc: cancel capture   •   Enter: apply   •   Tab: focus controls",
                         -1, &font, PointF(kSettingsPadX, y), &brush);
}

static void PaintSettingsWindow(HWND hwnd, HDC dc, const SettingsViewState &state) {
    RECT client;
    GetClientRect(hwnd, &client);

    HDC mem = CreateCompatibleDC(dc);
    HBITMAP buffer = CreateCompatibleBitmap(dc, client.right, client.bottom);
    HGDIOBJ oldBuf = SelectObject(mem, buffer);

    HBRUSH bg = CreateSolidBrush(RGB(22, 28, 33));
    FillRect(mem, &client, bg);
    DeleteObject(bg);

    if (StartGdiplus()) {
        Graphics graphics(mem);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        DrawSettingsTitle(&graphics);

        Pen rule(Color(255, 40, 52, 60), 1.0f);
        graphics.DrawLine(&rule, 0, 78, kSettingsClientW, 78);

        DrawSettingsSectionHeader(&graphics, L"HOTKEY", 90.0f - 12.0f);
        RECT hotkeyRect = SettingsHotkeyRect();
        DrawSettingsHotkeyCard(&graphics, hotkeyRect, state);

        DrawSettingsSectionHeader(&graphics, L"BEHAVIOR", 232.0f - 16.0f);
        RECT startupRect = SettingsStartupRect();
        DrawSettingsCheckRow(&graphics, startupRect, state);

        RECT bannerRect = SettingsBannerRect();
        DrawSettingsBanner(&graphics, bannerRect, state);

        DrawSettingsHelpHint(&graphics, static_cast<float>(kSettingsClientH - 90));

        bool applyHover = state.hover == SettingsHitApply;
        bool cancelHover = state.hover == SettingsHitCancel;
        bool applyPressed = state.pressed == SettingsHitApply;
        bool cancelPressed = state.pressed == SettingsHitCancel;
        DrawSettingsButton(&graphics, SettingsCancelRect(), L"Cancel", false, cancelHover, cancelPressed);
        DrawSettingsButton(&graphics, SettingsApplyRect(), L"Apply", true, applyHover, applyPressed);
    }

    BitBlt(dc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBuf);
    DeleteObject(buffer);
    DeleteDC(mem);
}

static void SetSettingsBanner(SettingsViewState *state, SettingsBanner kind, std::wstring text) {
    state->banner = kind;
    state->status = std::move(text);
}

static void BeginHotkeyCapture(SettingsViewState *state) {
    state->capturing = true;
    state->captureModifiers = 0;
    SetSettingsBanner(state, BannerInfo, L"Press the keys you want to use. Esc to cancel.");
}

static void FinishHotkeyCapture(SettingsViewState *state, UINT vk) {
    state->pending.modifiers = state->captureModifiers ? state->captureModifiers : MOD_CONTROL;
    state->pending.vk = vk;
    state->capturing = false;
    state->captureModifiers = 0;
    SetSettingsBanner(state, BannerInfo, L"New combination: " + HotkeyText(state->pending));
}

static void CancelHotkeyCapture(SettingsViewState *state) {
    state->capturing = false;
    state->captureModifiers = 0;
    SetSettingsBanner(state, BannerNone, L"");
}

static UINT ModifierBitForVk(UINT vk) {
    switch (vk) {
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return MOD_CONTROL;
    case VK_MENU:    case VK_LMENU:    case VK_RMENU:    return MOD_ALT;
    case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:   return MOD_SHIFT;
    case VK_LWIN:    case VK_RWIN:                       return MOD_WIN;
    }
    return 0;
}

static bool ApplySettingsFromView(HWND hwnd, SettingsViewState *state) {
    Settings prev = GetAppSettings();
    SetAppSettings(state->pending);
    HWND hiddenWnd = GetAppHiddenWindow();
    if (!RegisterConfiguredHotkey(hiddenWnd, false)) {
        SetAppSettings(prev);
        RegisterConfiguredHotkey(hiddenWnd, false);
        SetSettingsBanner(state, BannerWarning,
                          L"Hotkey " + HotkeyText(state->pending) + L" is already used by another app.");
        InvalidateRect(hwnd, NULL, FALSE);
        return false;
    }
    SaveSettings(GetAppSettings());
    if (!SetStartupEnabled(state->startup)) {
        SetSettingsBanner(state, BannerWarning,
                          L"Saved hotkey, but could not update Windows startup.");
        InvalidateRect(hwnd, NULL, FALSE);
        return false;
    }
    ShowTrayBalloon(hiddenWnd, L"Settings updated", HotkeyText(GetAppSettings()));
    return true;
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    SettingsViewState *state = reinterpret_cast<SettingsViewState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        SettingsViewState *view = new SettingsViewState();
        view->pending = GetAppSettings();
        view->startup = IsStartupEnabled();
        SetSettingsBanner(view, BannerInfo, L"Current hotkey: " + HotkeyText(GetAppSettings()));
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        SetClassLongPtrW(hwnd, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(LoadCursor(NULL, IDC_ARROW)));
        HICON icon = GetAppTrayIcon();
        if (icon) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (!state) return 0;
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        SettingsHit hit = SettingsHitTest(x, y);
        state->pressed = hit;
        SetCapture(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!state) return 0;
        ReleaseCapture();
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        SettingsHit hit = SettingsHitTest(x, y);
        SettingsHit pressed = state->pressed;
        state->pressed = SettingsHitNone;

        if (hit != SettingsHitNone && hit == pressed) {
            switch (hit) {
            case SettingsHitHotkey:
                if (state->capturing) {
                    CancelHotkeyCapture(state);
                } else {
                    BeginHotkeyCapture(state);
                }
                break;
            case SettingsHitStartup:
                state->startup = !state->startup;
                break;
            case SettingsHitApply:
                if (ApplySettingsFromView(hwnd, state)) {
                    DestroyWindow(hwnd);
                    return 0;
                }
                break;
            case SettingsHitCancel:
                DestroyWindow(hwnd);
                return 0;
            default:
                break;
            }
        }
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!state) return 0;
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        SettingsHit hit = SettingsHitTest(x, y);
        if (hit != state->hover) {
            state->hover = hit;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&track);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (state && state->hover != SettingsHitNone) {
            state->hover = SettingsHitNone;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
        if (!state) return 0;
        UINT vk = static_cast<UINT>(wParam);
        if (state->capturing) {
            if (vk == VK_ESCAPE) {
                CancelHotkeyCapture(state);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            UINT mod = ModifierBitForVk(vk);
            if (mod != 0) {
                state->captureModifiers |= mod;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (vk == VK_TAB) {
                CancelHotkeyCapture(state);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            FinishHotkeyCapture(state, vk);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (vk == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (vk == VK_RETURN) {
            if (ApplySettingsFromView(hwnd, state)) {
                DestroyWindow(hwnd);
                return 0;
            }
            return 0;
        }
        return 0;
    }
    case WM_SYSKEYUP:
    case WM_KEYUP: {
        if (!state) return 0;
        if (state->capturing) {
            UINT mod = ModifierBitForVk(static_cast<UINT>(wParam));
            if (mod != 0) {
                state->captureModifiers &= ~mod;
            }
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            PaintSettingsWindow(hwnd, dc, *state);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCDESTROY:
        if (state) {
            delete state;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        if (g_settingsWindow == hwnd) {
            g_settingsWindow = NULL;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ShowSettingsWindow(HINSTANCE instance, HICON trayIcon, HWND hiddenWnd) {
    if (g_settingsWindow && IsWindow(g_settingsWindow)) {
        ShowWindow(g_settingsWindow, SW_RESTORE);
        SetForegroundWindow(g_settingsWindow);
        return;
    }

    RECT wr = {0, 0, kSettingsClientW, kSettingsClientH};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRectEx(&wr, style, FALSE, WS_EX_APPWINDOW);

    int w = RectWidth(wr);
    int h = RectHeight(wr);
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_settingsWindow = CreateWindowExW(WS_EX_APPWINDOW, kSettingsClass, L"ModernScreenshot Settings",
                                       style, x, y, w, h, NULL, NULL, instance, NULL);
    if (!g_settingsWindow) return;

    ShowWindow(g_settingsWindow, SW_SHOW);
    UpdateWindow(g_settingsWindow);
    SetForegroundWindow(g_settingsWindow);
}

} // namespace ModernScreenshot
