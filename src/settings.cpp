#include "settings.h"
#include "app.h"

namespace ModernScreenshot {

static const int kSettingsClientW = 480;
static const int kSettingsClientH = 420;
static const int kSettingsPadX = 28;

static HWND g_settingsWindow = NULL;
static HBITMAP g_settingsBackdrop = NULL;

static RECT SettingsHotkeyRect() {
    RECT r = {kSettingsPadX, 92, kSettingsClientW - kSettingsPadX, 162};
    return r;
}

static RECT SettingsStartupRect() {
    RECT r = {kSettingsPadX, 224, kSettingsClientW - kSettingsPadX, 286};
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
    SolidBrush brush(Color(255, 255, 255, 255));
    graphics->DrawString(L"Settings", -1, &font, PointF(kSettingsPadX, 26), &brush);

    // Frosted glass version pill badge
    std::wstring version = L"v";
    version += kVersion;
    Font sub(L"Segoe UI", 11.0f, FontStyleBold, UnitPixel);
    SolidBrush subBrush(Color(255, 241, 245, 249));
    GraphicsPath badge;
    BuildRoundedRectPath(&badge, kSettingsPadX + 90.0f, 30.0f, 60.0f, 22.0f, 11.0f);
    SolidBrush badgeBg(Color(48, 255, 255, 255));
    Pen badgeBorder(Color(95, 255, 255, 255), 1.0f);
    graphics->FillPath(&badgeBg, &badge);
    graphics->DrawPath(&badgeBorder, &badge);
    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    RectF bRect(kSettingsPadX + 90.0f, 30.0f, 60.0f, 22.0f);
    graphics->DrawString(version.c_str(), -1, &sub, bRect, &fmt, &subBrush);
}

static void DrawSettingsSectionHeader(Graphics *graphics, const wchar_t *text, float y) {
    Font font(L"Segoe UI", 10.0f, FontStyleBold, UnitPixel);
    SolidBrush brush(Color(220, 203, 213, 225));
    graphics->DrawString(text, -1, &font, PointF(kSettingsPadX, y), &brush);
}

static void DrawSettingsHotkeyCard(HDC dc, Graphics *graphics, const RECT &rect, const SettingsViewState &state) {
    bool hover = state.hover == SettingsHitHotkey || state.capturing;
    DrawGlassButton(dc, rect, 12.0f, false, state.capturing, hover, false, false);

    Font hintFont(L"Segoe UI", 10.5f, FontStyleRegular, UnitPixel);
    SolidBrush hintBrush(Color(220, 203, 213, 225));
    graphics->DrawString(state.capturing ? L"Listening for keys..." : L"Global Capture Hotkey",
                         -1, &hintFont, PointF(rect.left + 16.0f, rect.top + 10.0f), &hintBrush);

    std::wstring label;
    if (state.capturing) {
        label = L"Press any combination (Esc to cancel)";
    } else {
        label = HotkeyText(state.pending);
    }

    Font keyFont(L"Segoe UI", 15.0f, FontStyleBold, UnitPixel);
    SolidBrush keyBrush(Color(255, 255, 255, 255));
    graphics->DrawString(label.c_str(), -1, &keyFont, PointF(rect.left + 16.0f, rect.top + 32.0f), &keyBrush);

    if (!state.capturing) {
        RectF pillRect(static_cast<float>(rect.right - 108), static_cast<float>(rect.top + 23), 94.0f, 26.0f);
        GraphicsPath pillPath;
        BuildRoundedRectPath(&pillPath, pillRect.X, pillRect.Y, pillRect.Width, pillRect.Height, 13.0f);
        SolidBrush pillFill(Color(hover ? 64 : 36, 255, 255, 255));
        Pen pillRim(Color(hover ? 130 : 80, 255, 255, 255), 1.0f);
        graphics->FillPath(&pillFill, &pillPath);
        graphics->DrawPath(&pillRim, &pillPath);

        Font edit(L"Segoe UI", 11.0f, FontStyleBold, UnitPixel);
        SolidBrush editBrush(Color(255, 255, 255, 255));
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        graphics->DrawString(L"Record key", -1, &edit, pillRect, &fmt, &editBrush);
    }
}

static void DrawSettingsCheckRow(HDC dc, Graphics *graphics, const RECT &rect, const SettingsViewState &state) {
    bool hover = state.hover == SettingsHitStartup;
    DrawGlassButton(dc, rect, 12.0f, false, false, hover, false, false);

    float bx = static_cast<float>(rect.left + 16);
    float by = static_cast<float>(rect.top + 11);

    Font label(L"Segoe UI", 13.0f, FontStyleBold, UnitPixel);
    SolidBrush labelBrush(Color(255, 255, 255, 255));
    graphics->DrawString(L"Start with Windows", -1, &label, PointF(bx, by), &labelBrush);

    Font desc(L"Segoe UI", 11.0f, FontStyleRegular, UnitPixel);
    SolidBrush descBrush(Color(220, 203, 213, 225));
    graphics->DrawString(L"Launch ModernScreenshot automatically when you sign in",
                         -1, &desc, PointF(bx, by + 22.0f), &descBrush);

    // Apple macOS / iOS Frosted Glass Toggle Switch on the right side
    float swW = 44.0f;
    float swH = 24.0f;
    float swX = static_cast<float>(rect.right) - swW - 16.0f;
    float swY = static_cast<float>(rect.top) + (static_cast<float>(RectHeight(rect)) - swH) / 2.0f;

    GraphicsPath trackPath;
    BuildRoundedRectPath(&trackPath, swX, swY, swW, swH, swH / 2.0f);
    Color trackTop = state.startup ? Color(215, 66, 130, 252) : Color(48, 255, 255, 255);
    Color trackBot = state.startup ? Color(200, 48, 96, 236) : Color(24, 255, 255, 255);
    Gdiplus::LinearGradientBrush trackGrad(PointF(swX, swY), PointF(swX, swY + swH), trackTop, trackBot);
    Pen trackBorder(state.startup ? Color(185, 190, 220, 255) : Color(95, 255, 255, 255), 1.0f);
    graphics->FillPath(&trackGrad, &trackPath);
    graphics->DrawPath(&trackBorder, &trackPath);

    float knobD = 18.0f;
    float knobX = state.startup ? (swX + swW - knobD - 3.0f) : (swX + 3.0f);
    float knobY = swY + 3.0f;
    SolidBrush knobShadow(Color(65, 0, 0, 0));
    SolidBrush knobFill(Color(255, 255, 255, 255));
    graphics->FillEllipse(&knobShadow, knobX, knobY + 1.0f, knobD, knobD);
    graphics->FillEllipse(&knobFill, knobX, knobY, knobD, knobD);
}

static void DrawSettingsBanner(Graphics *graphics, const RECT &rect, const SettingsViewState &state) {
    if (state.banner == BannerNone || state.status.empty()) {
        return;
    }
    Color fillColor, borderColor, textColor, glyphColor;
    const wchar_t *glyph = L"i";
    if (state.banner == BannerSuccess) {
        fillColor = Color(120, 16, 85, 62);
        borderColor = Color(190, 52, 211, 153);
        textColor = Color(255, 209, 250, 229);
        glyphColor = Color(255, 52, 211, 153);
        glyph = L"✓";
    } else if (state.banner == BannerWarning) {
        fillColor = Color(120, 110, 68, 18);
        borderColor = Color(190, 251, 191, 36);
        textColor = Color(255, 254, 243, 199);
        glyphColor = Color(255, 251, 191, 36);
        glyph = L"!";
    } else {
        fillColor = Color(52, 255, 255, 255);
        borderColor = Color(110, 255, 255, 255);
        textColor = Color(255, 241, 245, 249);
        glyphColor = Color(255, 125, 211, 252);
    }

    GraphicsPath path;
    BuildRoundedRectPath(&path, static_cast<float>(rect.left), static_cast<float>(rect.top),
                         static_cast<float>(RectWidth(rect)), static_cast<float>(RectHeight(rect)), 10.0f);
    SolidBrush fill(fillColor);
    Pen border(borderColor, 1.0f);
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

static void DrawSettingsButton(HDC dc, Graphics *graphics, const RECT &rect, const wchar_t *label,
                               bool primary, bool hovered, bool pressed) {
    DrawGlassButton(dc, rect, 10.0f, primary, false, hovered, pressed, false);

    Font font(L"Segoe UI", 13.0f, primary ? FontStyleBold : FontStyleRegular, UnitPixel);
    SolidBrush textBrush(Color(255, 255, 255, 255));
    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    RectF text(static_cast<float>(rect.left), static_cast<float>(rect.top),
               static_cast<float>(RectWidth(rect)), static_cast<float>(RectHeight(rect)));
    graphics->DrawString(label, -1, &font, text, &fmt, &textBrush);
}

static void DrawSettingsHelpHint(Graphics *graphics, float y) {
    Font font(L"Segoe UI", 10.5f, FontStyleRegular, UnitPixel);
    SolidBrush brush(Color(200, 180, 194, 212));
    graphics->DrawString(L"Esc: cancel capture   •   Enter: apply   •   Tab: focus controls",
                         -1, &font, PointF(kSettingsPadX, y), &brush);
}

static void PaintSettingsWindow(HWND hwnd, HDC dc, const SettingsViewState &state) {
    RECT client;
    GetClientRect(hwnd, &client);

    HDC mem = CreateCompatibleDC(dc);
    HBITMAP buffer = CreateCompatibleBitmap(dc, client.right, client.bottom);
    HGDIOBJ oldBuf = SelectObject(mem, buffer);

    FillRectColor(mem, client, RGB(16, 20, 32));

    if (g_settingsBackdrop) {
        HDC src = CreateCompatibleDC(dc);
        HGDIOBJ oldSrc = SelectObject(src, g_settingsBackdrop);
        BitBlt(mem, 0, 0, client.right, client.bottom, src, 0, 0, SRCCOPY);
        SelectObject(src, oldSrc);
        DeleteDC(src);
    }

    DrawGlassPanel(mem, client, 0.0f, true, false);

    if (StartGdiplus()) {
        Graphics graphics(mem);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        DrawSettingsTitle(&graphics);

        Pen rule(Color(55, 255, 255, 255), 1.0f);
        graphics.DrawLine(&rule, kSettingsPadX, 72, kSettingsClientW - kSettingsPadX, 72);

        DrawSettingsSectionHeader(&graphics, L"HOTKEY", 76.0f);
        RECT hotkeyRect = SettingsHotkeyRect();
        DrawSettingsHotkeyCard(mem, &graphics, hotkeyRect, state);

        DrawSettingsSectionHeader(&graphics, L"BEHAVIOR", 206.0f);
        RECT startupRect = SettingsStartupRect();
        DrawSettingsCheckRow(mem, &graphics, startupRect, state);

        RECT bannerRect = SettingsBannerRect();
        DrawSettingsBanner(&graphics, bannerRect, state);

        DrawSettingsHelpHint(&graphics, static_cast<float>(kSettingsClientH - 88));

        bool applyHover = state.hover == SettingsHitApply;
        bool cancelHover = state.hover == SettingsHitCancel;
        bool applyPressed = state.pressed == SettingsHitApply;
        bool cancelPressed = state.pressed == SettingsHitCancel;
        DrawSettingsButton(mem, &graphics, SettingsCancelRect(), L"Cancel", false, cancelHover, cancelPressed);
        DrawSettingsButton(mem, &graphics, SettingsApplyRect(), L"Apply", true, applyHover, applyPressed);
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
        ApplyWindowGlassStyle(hwnd, 0, 245);
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
        if (g_settingsBackdrop) {
            DeleteObject(g_settingsBackdrop);
            g_settingsBackdrop = NULL;
        }
        if (g_settingsWindow == hwnd) {
            g_settingsWindow = NULL;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void ShowSettingsWindow(HINSTANCE instance, HICON trayIcon, HWND hiddenWnd) {
    (void)trayIcon;
    (void)hiddenWnd;
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

    if (g_settingsBackdrop) {
        DeleteObject(g_settingsBackdrop);
        g_settingsBackdrop = NULL;
    }
    RECT captureRect = {x, y, x + kSettingsClientW, y + kSettingsClientH};
    g_settingsBackdrop = CaptureScreenRect(captureRect);

    g_settingsWindow = CreateWindowExW(WS_EX_APPWINDOW, kSettingsClass, L"ModernScreenshot Settings",
                                       style, x, y, w, h, NULL, NULL, instance, NULL);
    if (!g_settingsWindow) return;

    ShowWindow(g_settingsWindow, SW_SHOW);
    UpdateWindow(g_settingsWindow);
    SetForegroundWindow(g_settingsWindow);
}

} // namespace ModernScreenshot
