#include "app.h"
#include "overlay.h"
#include "long_capture.h"
#include "editor.h"
#include "tray_popup.h"
#include "settings.h"

namespace ModernScreenshot {

static HINSTANCE g_instance = NULL;
static HWND g_hiddenWindow = NULL;
static HICON g_trayIcon = NULL;
static Settings g_settings;
static bool g_hotkeyRegistered = false;
static bool g_captureActive = false;
static UINT g_wakeupMessage = 0;

HINSTANCE GetAppInstance() { return g_instance; }
HWND GetAppHiddenWindow() { return g_hiddenWindow; }
HICON GetAppTrayIcon() { return g_trayIcon; }
Settings GetAppSettings() { return g_settings; }
void SetAppSettings(const Settings &s) { g_settings = s; }

static HICON CreateTrayIcon() {
    if (!StartGdiplus()) {
        return LoadIcon(NULL, IDI_APPLICATION);
    }

    const int size = 48;
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
        return LoadIcon(NULL, IDI_APPLICATION);
    }

    std::memset(bits, 0, static_cast<size_t>(size) * static_cast<size_t>(size) * 4);

    {
        Bitmap canvas(size, size, size * 4, PixelFormat32bppARGB, static_cast<BYTE *>(bits));
        Graphics g(&canvas);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

        // 1. Apple macOS Squircle Glass Base (transparent outer corners)
        GraphicsPath squircle;
        BuildRoundedRectPath(&squircle, 3.0f, 3.0f, 42.0f, 42.0f, 11.5f);

        Gdiplus::LinearGradientBrush bgGrad(
            PointF(3.0f, 3.0f), PointF(45.0f, 45.0f),
            Color(250, 36, 168, 255),   // Top-left: Electric Sky-Blue
            Color(252, 67, 56, 202));   // Bottom-right: Deep Electric Indigo
        g.FillPath(&bgGrad, &squircle);

        // 2. Top-half Frosted Glass Specular Sheen
        GraphicsPath sheenPath;
        BuildRoundedRectPath(&sheenPath, 4.5f, 4.5f, 39.0f, 19.5f, 10.0f);
        Gdiplus::LinearGradientBrush sheenGrad(
            PointF(0.0f, 4.0f), PointF(0.0f, 24.0f),
            Color(115, 255, 255, 255),
            Color(5, 255, 255, 255));
        g.FillPath(&sheenGrad, &sheenPath);

        // 3. Specular Glass Outer Rim
        Gdiplus::LinearGradientBrush rimGrad(
            PointF(3.0f, 3.0f), PointF(3.0f, 45.0f),
            Color(220, 224, 242, 254),
            Color(130, 129, 140, 248));
        Pen rimPen(&rimGrad, 1.6f);
        g.DrawPath(&rimPen, &squircle);

        // 4. Inner Frosted Viewfinder Frame (4 rounded corner brackets)
        Pen shadowPen(Color(90, 0, 0, 0), 3.4f);
        shadowPen.SetStartCap(Gdiplus::LineCapRound);
        shadowPen.SetEndCap(Gdiplus::LineCapRound);
        shadowPen.SetLineJoin(Gdiplus::LineJoinRound);

        Pen bracketPen(Color(255, 255, 255, 255), 3.0f);
        bracketPen.SetStartCap(Gdiplus::LineCapRound);
        bracketPen.SetEndCap(Gdiplus::LineCapRound);
        bracketPen.SetLineJoin(Gdiplus::LineJoinRound);

        auto drawBrackets = [&](Pen *p, float dy) {
            float L = 11.5f, T = 11.5f + dy, R = 36.5f, B = 36.5f + dy;
            float arm = 7.5f;
            // Top-left
            g.DrawLine(p, L, T + arm, L, T);
            g.DrawLine(p, L, T, L + arm, T);
            // Top-right
            g.DrawLine(p, R - arm, T, R, T);
            g.DrawLine(p, R, T, R, T + arm);
            // Bottom-left
            g.DrawLine(p, L, B - arm, L, B);
            g.DrawLine(p, L, B, L + arm, B);
            // Bottom-right
            g.DrawLine(p, R - arm, B, R, B);
            g.DrawLine(p, R, B - arm, R, B);
        };
        drawBrackets(&shadowPen, 1.0f);
        drawBrackets(&bracketPen, 0.0f);

        // 5. Center Luminous Glass Lens Core
        SolidBrush lensGlow(Color(85, 255, 255, 255));
        g.FillEllipse(&lensGlow, 17.5f, 17.5f, 13.0f, 13.0f);
        Pen lensRing(Color(245, 255, 255, 255), 2.0f);
        g.DrawEllipse(&lensRing, 18.5f, 18.5f, 11.0f, 11.0f);
        SolidBrush lensDot(Color(255, 125, 211, 252));
        g.FillEllipse(&lensDot, 21.5f, 21.5f, 5.0f, 5.0f);
    }

    std::vector<BYTE> maskBits(size * size / 8, 0);
    HBITMAP maskBmp = CreateBitmap(size, size, 1, 1, maskBits.data());

    ICONINFO info = {};
    info.fIcon = TRUE;
    info.hbmColor = colorBmp;
    info.hbmMask = maskBmp;
    HICON icon = CreateIconIndirect(&info);
    DeleteObject(colorBmp);
    DeleteObject(maskBmp);
    return icon ? icon : LoadIcon(NULL, IDI_APPLICATION);
}

void ShowTrayBalloon(HWND hwnd, const std::wstring &title, const std::wstring &message) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    lstrcpynW(nid.szInfoTitle, title.c_str(), ARRAYSIZE(nid.szInfoTitle));
    lstrcpynW(nid.szInfo, message.c_str(), ARRAYSIZE(nid.szInfo));
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void NotifyAction(HWND owner, const std::wstring &title, const std::wstring &message) {
    if (g_hiddenWindow) {
        ShowTrayBalloon(g_hiddenWindow, title, message);
    } else {
        MessageBoxW(owner, message.c_str(), title.c_str(), MB_OK);
    }
}

static bool AddTrayIcon(HWND hwnd) {
    g_trayIcon = CreateTrayIcon();
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kTrayMessage;
    nid.hIcon = g_trayIcon;
    lstrcpynW(nid.szTip, L"ModernScreenshot", ARRAYSIZE(nid.szTip));
    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        return false;
    }
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
    return true;
}

static void RemoveTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    if (g_trayIcon) {
        DestroyIcon(g_trayIcon);
        g_trayIcon = NULL;
    }
}

bool RegisterConfiguredHotkey(HWND hwnd, bool notify) {
    if (g_hotkeyRegistered) {
        UnregisterHotKey(hwnd, kHotkeyId);
        g_hotkeyRegistered = false;
    }

    UINT modifiers = g_settings.modifiers | MOD_NOREPEAT;
    if (RegisterHotKey(hwnd, kHotkeyId, modifiers, g_settings.vk)) {
        g_hotkeyRegistered = true;
        return true;
    }

    if (notify) {
        ShowTrayBalloon(hwnd, L"Hotkey conflict",
                        HotkeyText(g_settings) + L" is already in use. Open Settings from the tray icon to change it.");
    }
    return false;
}

void RunSelectedCapture(CaptureKind initialKind) {
    if (g_captureActive) return;
    g_captureActive = true;
    HWND previousForeground = GetForegroundWindow();
    ScreenCapture capture = CaptureVirtualScreen();
    if (!capture.bitmap) {
        g_captureActive = false;
        return;
    }

    RECT selection;
    CaptureKind kind = initialKind;
    if (RunSelectionOverlay(g_instance, capture, &selection, &kind, true, initialKind)) {
        if (kind == CaptureLongRegion) {
            DeleteObject(capture.bitmap);
            capture.bitmap = NULL;
            Sleep(120);

            int longH = 0;
            HBITMAP shot = RunLongSession(g_instance, selection, previousForeground, &longH);
            if (shot) {
                RunEditorModal(g_instance, shot, RectWidth(selection), longH);
                DeleteObject(shot);
            } else {
                NotifyAction(g_hiddenWindow, L"Long capture cancelled",
                             L"No long screenshot was created.");
            }
        } else {
            HBITMAP shot = CropCapture(capture, selection);
            if (shot) {
                RunEditorModal(g_instance, shot, RectWidth(selection), RectHeight(selection));
                DeleteObject(shot);
            }
        }
    }
    if (capture.bitmap) {
        DeleteObject(capture.bitmap);
    }
    g_captureActive = false;
}

void RunCapture() {
    RunSelectedCapture(CaptureRegion);
}

void RunLongCapture() {
    RunSelectedCapture(CaptureLongRegion);
}

bool SelfTestExport(const std::wstring &path) {
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
    if (output) DeleteObject(output);
    DeleteObject(bitmap);
    return ok;
}

static LRESULT CALLBACK HiddenProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (g_wakeupMessage && message == g_wakeupMessage) {
        ShowSettingsWindow(g_instance, g_trayIcon, hwnd);
        return 0;
    }
    switch (message) {
    case kTrayMessage:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            ShowTrayPopup(hwnd, g_instance, hwnd, g_settings);
        } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            RunCapture();
        }
        return 0;
    case WM_HOTKEY:
        if (wParam == kHotkeyId && !g_captureActive) {
            RunCapture();
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == kMenuCapture) {
            RunCapture();
        } else if (LOWORD(wParam) == kMenuLongCapture) {
            RunLongCapture();
        } else if (LOWORD(wParam) == kMenuSettings) {
            ShowSettingsWindow(g_instance, g_trayIcon, hwnd);
        } else if (LOWORD(wParam) == kMenuExit) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        if (g_hotkeyRegistered) {
            UnregisterHotKey(hwnd, kHotkeyId);
            g_hotkeyRegistered = false;
        }
        RemoveTrayIcon(hwnd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void RegisterWindowClasses(HINSTANCE instance) {
    g_instance = instance;

    WNDCLASSW hidden = {};
    hidden.lpfnWndProc = HiddenProc;
    hidden.hInstance = instance;
    hidden.lpszClassName = kHiddenClass;
    RegisterClassW(&hidden);

    WNDCLASSW overlay = {};
    overlay.lpfnWndProc = OverlayProc;
    overlay.hInstance = instance;
    overlay.hCursor = LoadCursor(NULL, IDC_CROSS);
    overlay.hbrBackground = NULL;
    overlay.lpszClassName = kOverlayClass;
    RegisterClassW(&overlay);

    WNDCLASSW longSession = {};
    longSession.lpfnWndProc = LongSessionProc;
    longSession.hInstance = instance;
    longSession.hCursor = LoadCursor(NULL, IDC_ARROW);
    longSession.hbrBackground = NULL;
    longSession.lpszClassName = kLongSessionClass;
    RegisterClassW(&longSession);

    WNDCLASSW editor = {};
    editor.lpfnWndProc = EditorProc;
    editor.hInstance = instance;
    editor.hCursor = LoadCursor(NULL, IDC_ARROW);
    editor.hbrBackground = NULL;
    editor.lpszClassName = kEditorClass;
    RegisterClassW(&editor);

    WNDCLASSW settings = {};
    settings.lpfnWndProc = SettingsProc;
    settings.hInstance = instance;
    settings.hCursor = LoadCursor(NULL, IDC_ARROW);
    settings.hbrBackground = NULL;
    settings.lpszClassName = kSettingsClass;
    RegisterClassW(&settings);

    WNDCLASSW trayPopup = {};
    trayPopup.lpfnWndProc = TrayPopupProc;
    trayPopup.hInstance = instance;
    trayPopup.hCursor = LoadCursor(NULL, IDC_ARROW);
    trayPopup.hbrBackground = NULL;
    trayPopup.lpszClassName = kTrayPopupClass;
    trayPopup.style = CS_DROPSHADOW;
    RegisterClassW(&trayPopup);
}

void UnregisterWindowClasses(HINSTANCE instance) {
    UnregisterClassW(kTrayPopupClass, instance);
    UnregisterClassW(kSettingsClass, instance);
    UnregisterClassW(kEditorClass, instance);
    UnregisterClassW(kLongSessionClass, instance);
    UnregisterClassW(kOverlayClass, instance);
    UnregisterClassW(kHiddenClass, instance);
}

bool PrintVersionToConsole() {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        return false;
    }
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out == INVALID_HANDLE_VALUE || out == NULL) {
        FreeConsole();
        return false;
    }
    std::wstring line = L"ModernScreenshot ";
    line += kVersion;
    line += L"\r\n";
    DWORD written = 0;
    WriteConsoleW(out, line.c_str(), static_cast<DWORD>(line.size()), &written, NULL);
    FreeConsole();
    return true;
}

bool WakePreviousInstance() {
    HWND existing = FindWindowExW(HWND_MESSAGE, NULL, kHiddenClass, NULL);
    if (!existing) return false;
    if (!g_wakeupMessage) {
        g_wakeupMessage = RegisterWindowMessageW(kWakeupMessageName);
    }
    if (g_wakeupMessage) {
        PostMessageW(existing, g_wakeupMessage, 0, 0);
    }
    return true;
}

int RunDaemon(HINSTANCE instance) {
    HWND hwnd = CreateWindowExW(0, kHiddenClass, L"ModernScreenshot", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, instance, NULL);
    if (!hwnd) return 1;
    g_hiddenWindow = hwnd;
    AddTrayIcon(hwnd);
    EnsureStartupPathSynced();

    if (!RegisterConfiguredHotkey(hwnd, true)) {
        ShowSettingsWindow(instance, g_trayIcon, hwnd);
    } else {
        ShowTrayBalloon(hwnd, L"ModernScreenshot is running", L"Use " + HotkeyText(g_settings) + L" or the tray menu.");
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

} // namespace ModernScreenshot
