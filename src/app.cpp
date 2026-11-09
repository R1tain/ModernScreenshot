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
    HDC screen = GetDC(NULL);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP color = CreateCompatibleBitmap(screen, 32, 32);
    std::vector<BYTE> maskBits(32 * 32 / 8, 0);
    HBITMAP mask = CreateBitmap(32, 32, 1, 1, maskBits.data());
    HGDIOBJ old = SelectObject(dc, color);

    RECT bg = {0, 0, 32, 32};
    FillRectColor(dc, bg, RGB(13, 15, 18));
    HBRUSH accent = CreateSolidBrush(RGB(99, 102, 241));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, accent));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(248, 250, 252));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, 5, 6, 27, 24, 7, 7);
    MoveToEx(dc, 11, 15, NULL);
    LineTo(dc, 21, 15);
    MoveToEx(dc, 16, 10, NULL);
    LineTo(dc, 16, 20);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(accent);
    SelectObject(dc, old);
    DeleteDC(dc);
    ReleaseDC(NULL, screen);

    ICONINFO info = {};
    info.fIcon = TRUE;
    info.hbmColor = color;
    info.hbmMask = mask;
    HICON icon = CreateIconIndirect(&info);
    DeleteObject(color);
    DeleteObject(mask);
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

    WNDCLASSW longPreview = {};
    longPreview.lpfnWndProc = LongPreviewProc;
    longPreview.hInstance = instance;
    longPreview.hCursor = LoadCursor(NULL, IDC_ARROW);
    longPreview.hbrBackground = NULL;
    longPreview.lpszClassName = kLongPreviewClass;
    RegisterClassW(&longPreview);

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
    UnregisterClassW(kLongPreviewClass, instance);
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
