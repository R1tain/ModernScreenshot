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
#include <cstring>
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
using Gdiplus::GraphicsPath;
using Gdiplus::ImageCodecInfo;
using Gdiplus::Pen;
using Gdiplus::PointF;
using Gdiplus::RectF;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
using Gdiplus::UnitPixel;

static const wchar_t *kVersion = WIDEN(VERSION);
static const wchar_t *kHiddenClass = L"ModernScreenshotHidden";
static const wchar_t *kOverlayClass = L"ModernScreenshotOverlay";
static const wchar_t *kLongSessionClass = L"ModernScreenshotLongSession";
static const wchar_t *kEditorClass = L"ModernScreenshotEditor";
static const wchar_t *kSettingsClass = L"ModernScreenshotSettings";
static const wchar_t *kTrayPopupClass = L"ModernScreenshotTrayPopup";
static const int kHotkeyId = 1001;
static const UINT kTrayMessage = WM_APP + 1;
static const UINT kMenuCapture = 2001;
static const UINT kMenuLongCapture = 2002;
static const UINT kMenuSettings = 2003;
static const UINT kMenuExit = 2004;
static const int kToolbarH = 64;
static const int kButtonW = 58;
static const int kButtonH = 46;
static const int kButtonGap = 8;
static const int kEditorMargin = 80;
static const int kLongCaptureMaxFrames = 256;
static const int kLongCaptureMaxHeight = 60000;
static const int kLongScrollWaitMs = 260;
static const int kLongScrollSettleMs = 80;
static const int kLongScrollSettleAttempts = 5;
static const int kLongOverlapErrorLimit = 30;
static const int kLongSessionW = 360;
static const int kLongSessionH = 520;
static const int kSettingsApply = 3001;
static const int kSettingsCancel = 3002;
static const int kSettingsHotkeyCapture = 3003;
static const int kSettingsStartup = 3009;
static const int kOverlayModeW = 116;
static const int kOverlayModeH = 34;
static const int kOverlayModeGap = 8;
static const int kLongSessionDone = 4101;
static const int kLongSessionCancel = 4102;

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

enum CaptureKind {
    CaptureRegion,
    CaptureLongRegion,
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

struct LongFrame {
    HBITMAP bitmap = NULL;
    int copyH = 0;
};

struct OverlayState {
    ScreenCapture capture;
    bool showModeButtons = false;
    CaptureKind kind = CaptureRegion;
    bool dragging = false;
    bool done = false;
    bool cancelled = false;
    POINT start = {0, 0};
    POINT current = {0, 0};
    RECT selection = {0, 0, 0, 0};
};

struct LongSessionState {
    RECT selection = {0, 0, 0, 0};
    bool done = false;
    bool cancelled = false;
    bool started = false;
    bool hasPrev = false;
    int w = 0;
    int h = 0;
    int totalH = 0;
    int skippedFrames = 0;
    int lastCopyH = 0;
    int unchangedSteps = 0;
    bool captureLimitReached = false;
    bool lastSampleUnchanged = false;
    HBITMAP pendingBitmap = NULL;
    int pendingStableFrames = 0;
    std::wstring statusText;
    HWND target = NULL;
    HWND targetRoot = NULL;
    std::vector<LongFrame> frames;
    std::vector<BYTE> prevPixels;
    std::vector<BYTE> pendingPixels;
};

struct EditorState {
    HBITMAP shot = NULL;
    int w = 0;
    int h = 0;
    int winW = 0;
    int winH = 0;
    int imageX = 0;
    int imageY = 0;
    int viewH = 0;
    int scrollY = 0;
    Tool activeTool = ToolRect;
    std::vector<Annotation> ops;
    bool drawing = false;
    Annotation preview;
    bool textEntry = false;
    POINT textPoint = {0, 0};
    std::wstring textBuffer;
    std::wstring statusText;
    bool done = false;
};

struct Settings {
    UINT modifiers = MOD_CONTROL;
    UINT vk = '1';
};

struct KeyOption {
    const wchar_t *label;
    UINT vk;
};

static ULONG_PTR g_gdiplusToken = 0;
static HINSTANCE g_instance = NULL;
static HWND g_hiddenWindow = NULL;
static HWND g_settingsWindow = NULL;
static HICON g_trayIcon = NULL;
static Settings g_settings;
static bool g_hotkeyRegistered = false;
static bool g_captureActive = false;
static HANDLE g_singleInstanceMutex = NULL;
static const wchar_t *kSingleInstanceName = L"Local\\ModernScreenshot.SingleInstance";
static const wchar_t *kWakeupMessageName = L"ModernScreenshotWakeup";
static UINT g_wakeupMessage = 0;

static void ShowTrayBalloon(HWND hwnd, const std::wstring &title, const std::wstring &message);

static const ButtonSpec kButtons[] = {
    {ActionToolBlur, ToolBlur, L"Blur"},
    {ActionToolArrow, ToolArrow, L"Arrow"},
    {ActionToolRect, ToolRect, L"Box"},
    {ActionToolText, ToolText, L"Text"},
    {ActionUndo, ToolBlur, L"Undo"},
    {ActionCopy, ToolBlur, L"Copy"},
    {ActionSave, ToolBlur, L"Save"},
};

static const KeyOption kKeyOptions[] = {
    {L"1", '1'}, {L"2", '2'}, {L"3", '3'}, {L"4", '4'}, {L"5", '5'},
    {L"A", 'A'}, {L"B", 'B'}, {L"C", 'C'}, {L"D", 'D'}, {L"E", 'E'}, {L"F", 'F'},
    {L"G", 'G'}, {L"H", 'H'}, {L"I", 'I'}, {L"J", 'J'}, {L"K", 'K'}, {L"L", 'L'},
    {L"M", 'M'}, {L"N", 'N'}, {L"O", 'O'}, {L"P", 'P'}, {L"Q", 'Q'}, {L"R", 'R'},
    {L"S", 'S'}, {L"T", 'T'}, {L"U", 'U'}, {L"V", 'V'}, {L"W", 'W'}, {L"X", 'X'},
    {L"Y", 'Y'}, {L"Z", 'Z'},
    {L"F1", VK_F1}, {L"F2", VK_F2}, {L"F3", VK_F3}, {L"F4", VK_F4},
    {L"F5", VK_F5}, {L"F6", VK_F6}, {L"F7", VK_F7}, {L"F8", VK_F8},
    {L"F9", VK_F9}, {L"F10", VK_F10}, {L"F11", VK_F11}, {L"F12", VK_F12},
    {L"PrintScreen", VK_SNAPSHOT},
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

static RECT OverlayModeRect(int captureW, int captureH, int index) {
    int totalW = 2 * kOverlayModeW + kOverlayModeGap;
    int startX = (captureW - totalW) / 2;
    if (startX < 16) startX = 16;
    int y = captureH - 96;
    if (y < 16) y = 16;
    RECT rect;
    rect.left = startX + index * (kOverlayModeW + kOverlayModeGap);
    rect.top = y;
    rect.right = rect.left + kOverlayModeW;
    rect.bottom = rect.top + kOverlayModeH;
    return rect;
}

static RECT OverlayHintRect(int captureW, int captureH) {
    int w = 360;
    int h = 36;
    int x = (captureW - w) / 2;
    if (x < 16) x = 16;
    int y = captureH - 96 - h - 14;
    if (y < 16) y = 16;
    RECT rect = {x, y, x + w, y + h};
    return rect;
}

static bool PointInRect(int x, int y, const RECT &rect) {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

static bool RectIntersects(const RECT &a, const RECT &b) {
    RECT intersection;
    return IntersectRect(&intersection, &a, &b) != FALSE;
}

static HWND RootWindow(HWND hwnd) {
    return hwnd ? GetAncestor(hwnd, GA_ROOT) : NULL;
}

static CaptureKind OverlayModeAt(int captureW, int captureH, int x, int y, bool *hit) {
    RECT region = OverlayModeRect(captureW, captureH, 0);
    if (PointInRect(x, y, region)) {
        *hit = true;
        return CaptureRegion;
    }

    RECT longRegion = OverlayModeRect(captureW, captureH, 1);
    if (PointInRect(x, y, longRegion)) {
        *hit = true;
        return CaptureLongRegion;
    }

    *hit = false;
    return CaptureRegion;
}

static bool IsRectEmptyOrInvalid(const RECT &rect) {
    return rect.right <= rect.left || rect.bottom <= rect.top;
}

static RECT ExpandedRect(RECT rect, int amount, int maxW, int maxH) {
    rect.left = Clamp(rect.left - amount, 0, maxW);
    rect.top = Clamp(rect.top - amount, 0, maxH);
    rect.right = Clamp(rect.right + amount, 0, maxW);
    rect.bottom = Clamp(rect.bottom + amount, 0, maxH);
    return rect;
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

static int KeyOptionCount() {
    return static_cast<int>(sizeof(kKeyOptions) / sizeof(kKeyOptions[0]));
}

static std::wstring ConfigPath() {
    PWSTR appData = NULL;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appData))) {
        dir.assign(appData);
        CoTaskMemFree(appData);
    } else {
        wchar_t temp[MAX_PATH];
        GetTempPathW(MAX_PATH, temp);
        dir.assign(temp);
    }

    if (!dir.empty() && dir.back() != L'\\') {
        dir.push_back(L'\\');
    }
    dir += L"ModernScreenshot";
    CreateDirectoryW(dir.c_str(), NULL);
    return dir + L"\\settings.ini";
}

static std::wstring VkName(UINT vk) {
    for (int i = 0; i < KeyOptionCount(); ++i) {
        if (kKeyOptions[i].vk == vk) {
            return kKeyOptions[i].label;
        }
    }

    wchar_t text[64] = {};
    UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16;
    if (GetKeyNameTextW(static_cast<LONG>(scan), text, 64) > 0) {
        return text;
    }
    wchar_t fallback[16];
    wsprintfW(fallback, L"0x%02X", vk);
    return fallback;
}

static std::wstring HotkeyText(const Settings &settings) {
    std::wstring text;
    if (settings.modifiers & MOD_CONTROL) {
        text += L"Ctrl+";
    }
    if (settings.modifiers & MOD_ALT) {
        text += L"Alt+";
    }
    if (settings.modifiers & MOD_SHIFT) {
        text += L"Shift+";
    }
    if (settings.modifiers & MOD_WIN) {
        text += L"Win+";
    }
    text += VkName(settings.vk);
    return text;
}

static void LoadSettings() {
    std::wstring path = ConfigPath();
    g_settings.modifiers = GetPrivateProfileIntW(L"hotkey", L"modifiers", MOD_CONTROL, path.c_str());
    g_settings.vk = GetPrivateProfileIntW(L"hotkey", L"vk", '1', path.c_str());
    if ((g_settings.modifiers & (MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN)) == 0) {
        g_settings.modifiers = MOD_CONTROL;
    }
}

static void SaveSettings() {
    std::wstring path = ConfigPath();
    wchar_t value[32];
    wsprintfW(value, L"%u", g_settings.modifiers);
    WritePrivateProfileStringW(L"hotkey", L"modifiers", value, path.c_str());
    wsprintfW(value, L"%u", g_settings.vk);
    WritePrivateProfileStringW(L"hotkey", L"vk", value, path.c_str());
}

static std::wstring ModulePath() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return path;
}

static std::wstring QuotedPath(const std::wstring &path) {
    return L"\"" + path + L"\"";
}

static bool IsStartupEnabled() {
    HKEY key = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    wchar_t value[MAX_PATH * 2] = {};
    DWORD bytes = sizeof(value);
    DWORD type = 0;
    LONG status = RegQueryValueExW(key, L"ModernScreenshot", NULL, &type,
                                   reinterpret_cast<LPBYTE>(value), &bytes);
    RegCloseKey(key);
    return status == ERROR_SUCCESS && type == REG_SZ;
}

static bool SetStartupEnabled(bool enabled) {
    HKEY key = NULL;
    const wchar_t *runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (enabled) {
        if (RegCreateKeyExW(HKEY_CURRENT_USER, runKey, 0, NULL, 0, KEY_SET_VALUE,
                            NULL, &key, NULL) != ERROR_SUCCESS) {
            return false;
        }
        std::wstring command = QuotedPath(ModulePath());
        LONG status = RegSetValueExW(key, L"ModernScreenshot", 0, REG_SZ,
                                     reinterpret_cast<const BYTE *>(command.c_str()),
                                     static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
        return status == ERROR_SUCCESS;
    }

    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return true;
    }
    LONG status = RegDeleteValueW(key, L"ModernScreenshot");
    RegCloseKey(key);
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}

static void EnsureStartupPathSynced() {
    HKEY key = NULL;
    const wchar_t *runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_READ | KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return;
    }
    wchar_t stored[MAX_PATH * 2] = {};
    DWORD bytes = sizeof(stored);
    DWORD type = 0;
    LONG status = RegQueryValueExW(key, L"ModernScreenshot", NULL, &type,
                                   reinterpret_cast<LPBYTE>(stored), &bytes);
    if (status != ERROR_SUCCESS || type != REG_SZ) {
        RegCloseKey(key);
        return;
    }
    std::wstring expected = QuotedPath(ModulePath());
    if (expected != stored) {
        RegSetValueExW(key, L"ModernScreenshot", 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(expected.c_str()),
                       static_cast<DWORD>((expected.size() + 1) * sizeof(wchar_t)));
    }
    RegCloseKey(key);
}

static void BuildRoundedRectPath(GraphicsPath *path, float x, float y, float w, float h, float radius) {
    float d = radius * 2.0f;
    path->Reset();
    path->AddArc(x, y, d, d, 180.0f, 90.0f);
    path->AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path->AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path->AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path->CloseFigure();
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

    SYSTEMTIME localTime;
    GetLocalTime(&localTime);
    wchar_t stamp[48];
    wsprintfW(stamp, L"%04u%02u%02u-%02u%02u%02u-%03u",
              localTime.wYear, localTime.wMonth, localTime.wDay,
              localTime.wHour, localTime.wMinute, localTime.wSecond,
              localTime.wMilliseconds);
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

static HBITMAP CaptureScreenRect(const RECT &rect) {
    int w = RectWidth(rect);
    int h = RectHeight(rect);
    if (w <= 0 || h <= 0) {
        return NULL;
    }

    HDC screen = GetDC(NULL);
    HDC dest = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, w, h);
    if (!dest || !bitmap) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        if (dest) {
            DeleteDC(dest);
        }
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

static HBITMAP CopyBitmapStrip(HBITMAP sourceBitmap, int w, int sourceY, int copyH) {
    if (!sourceBitmap || w <= 0 || copyH <= 0) {
        return NULL;
    }

    HDC screen = GetDC(NULL);
    HDC source = CreateCompatibleDC(screen);
    HDC dest = CreateCompatibleDC(screen);
    HBITMAP strip = CreateCompatibleBitmap(screen, w, copyH);
    if (!source || !dest || !strip) {
        if (strip) {
            DeleteObject(strip);
        }
        if (source) {
            DeleteDC(source);
        }
        if (dest) {
            DeleteDC(dest);
        }
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

static bool ReadBitmapPixels(HBITMAP bitmap, int w, int h, std::vector<BYTE> *pixels) {
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

static unsigned int PixelAt(const std::vector<BYTE> &pixels, int w, int x, int y) {
    size_t index = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4;
    return static_cast<unsigned int>(pixels[index]) |
           (static_cast<unsigned int>(pixels[index + 1]) << 8) |
           (static_cast<unsigned int>(pixels[index + 2]) << 16);
}

static int PixelAbsDiff(unsigned int a, unsigned int b) {
    int db = static_cast<int>(a & 0xff) - static_cast<int>(b & 0xff);
    int dg = static_cast<int>((a >> 8) & 0xff) - static_cast<int>((b >> 8) & 0xff);
    int dr = static_cast<int>((a >> 16) & 0xff) - static_cast<int>((b >> 16) & 0xff);
    return std::abs(db) + std::abs(dg) + std::abs(dr);
}

static int AverageFrameDiff(const std::vector<BYTE> &a, const std::vector<BYTE> &b, int w, int h) {
    int xStep = std::max(1, w / 32);
    int yStep = std::max(1, h / 32);
    long long diff = 0;
    int samples = 0;
    for (int y = 0; y < h; y += yStep) {
        for (int x = 0; x < w; x += xStep) {
            diff += PixelAbsDiff(PixelAt(a, w, x, y), PixelAt(b, w, x, y));
            ++samples;
        }
    }
    return samples > 0 ? static_cast<int>(diff / samples) : 255;
}

static bool IsSameFramePixels(const std::vector<BYTE> &a, const std::vector<BYTE> &b) {
    return a.size() == b.size() &&
           (a.empty() || std::memcmp(a.data(), b.data(), a.size()) == 0);
}

static int OverlapError(const std::vector<BYTE> &prev, const std::vector<BYTE> &next, int w, int h, int overlap) {
    int xStep = std::max(1, w / 56);
    int yStep = std::max(1, overlap / 56);
    long long diff = 0;
    int samples = 0;
    for (int y = 0; y < overlap; y += yStep) {
        int prevY = h - overlap + y;
        for (int x = 0; x < w; x += xStep) {
            diff += PixelAbsDiff(PixelAt(prev, w, x, prevY), PixelAt(next, w, x, y));
            ++samples;
        }
    }
    return samples > 0 ? static_cast<int>(diff / samples) : 255;
}

static int EstimateOverlap(const std::vector<BYTE> &prev, const std::vector<BYTE> &next, int w, int h, int *outError) {
    if (w <= 0 || h <= 1) {
        if (outError) {
            *outError = 255 * 3;
        }
        return 0;
    }

    int minOverlap = std::min(h - 1, std::max(48, h / 6));
    int maxOverlap = h - 1;
    int step = std::max(1, h / 120);
    int bestOverlap = minOverlap;
    int bestError = 255 * 3;

    for (int overlap = minOverlap; overlap <= maxOverlap; overlap += step) {
        int error = OverlapError(prev, next, w, h, overlap);
        if (error < bestError) {
            bestError = error;
            bestOverlap = overlap;
        }
    }

    int refineStart = std::max(minOverlap, bestOverlap - step);
    int refineEnd = std::min(maxOverlap, bestOverlap + step);
    for (int overlap = refineStart; overlap <= refineEnd; ++overlap) {
        int error = OverlapError(prev, next, w, h, overlap);
        if (error < bestError) {
            bestError = error;
            bestOverlap = overlap;
        }
    }

    int tailStart = std::max(minOverlap, h - std::max(96, h / 6));
    for (int overlap = tailStart; overlap <= maxOverlap; ++overlap) {
        int error = OverlapError(prev, next, w, h, overlap);
        if (error < bestError) {
            bestError = error;
            bestOverlap = overlap;
        }
    }

    if (outError) {
        *outError = bestError;
    }
    return bestError <= kLongOverlapErrorLimit ? bestOverlap : 0;
}

static HBITMAP StitchLongFrames(const std::vector<LongFrame> &frames, int w, int *outH) {
    int totalH = 0;
    for (const LongFrame &frame : frames) {
        totalH += frame.copyH;
    }
    if (frames.empty() || w <= 0 || totalH <= 0) {
        return NULL;
    }

    HDC screen = GetDC(NULL);
    HDC source = CreateCompatibleDC(screen);
    HDC dest = CreateCompatibleDC(screen);
    HBITMAP output = CreateCompatibleBitmap(screen, w, totalH);
    if (!source || !dest || !output) {
        if (output) {
            DeleteObject(output);
        }
        if (source) {
            DeleteDC(source);
        }
        if (dest) {
            DeleteDC(dest);
        }
        ReleaseDC(NULL, screen);
        return NULL;
    }

    HGDIOBJ oldDest = SelectObject(dest, output);
    int y = 0;
    for (const LongFrame &frame : frames) {
        HGDIOBJ oldSource = SelectObject(source, frame.bitmap);
        BitBlt(dest, 0, y, w, frame.copyH, source, 0, 0, SRCCOPY);
        SelectObject(source, oldSource);
        y += frame.copyH;
    }

    SelectObject(dest, oldDest);
    DeleteDC(source);
    DeleteDC(dest);
    ReleaseDC(NULL, screen);
    if (outH) {
        *outH = totalH;
    }
    return output;
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

static void DrawCenteredText(HDC dc, const wchar_t *text, const RECT &rect, int fontHeight, int weight, COLORREF color) {
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

static void DrawOverlayModeButton(HDC dc, const RECT &rect, const wchar_t *text, bool active, int offsetX, int offsetY) {
    RECT drawRect = {rect.left - offsetX, rect.top - offsetY, rect.right - offsetX, rect.bottom - offsetY};
    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        GraphicsPath path;
        BuildRoundedRectPath(&path, static_cast<float>(drawRect.left), static_cast<float>(drawRect.top),
                             static_cast<float>(RectWidth(drawRect)), static_cast<float>(RectHeight(drawRect)), 10.0f);
        SolidBrush fill(active ? Color(245, 23, 198, 163) : Color(230, 16, 21, 24));
        Pen outline(active ? Color(255, 245, 251, 248) : Color(150, 82, 98, 100), active ? 1.4f : 1.0f);
        graphics.FillPath(&fill, &path);
        graphics.DrawPath(&outline, &path);
    } else {
        FillRectColor(dc, drawRect, active ? RGB(23, 198, 163) : RGB(16, 21, 24));
    }
    DrawCenteredText(dc, text, drawRect, -14, FW_SEMIBOLD, active ? RGB(7, 11, 14) : RGB(245, 251, 248));
}

static void DrawOverlayOffset(OverlayState *state, HWND hwnd, HDC dc, int offsetX, int offsetY) {
    HDC source = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(source, state->capture.bitmap);
    BitBlt(dc, -offsetX, -offsetY, state->capture.w, state->capture.h, source, 0, 0, SRCCOPY);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush shade(Color(138, 7, 11, 14));
        graphics.FillRectangle(&shade, 0, 0, state->capture.w, state->capture.h);
    }

    if (state->showModeButtons) {
        int cw = state->capture.w;
        int ch = state->capture.h;
        RECT hintBg = OverlayHintRect(cw, ch);
        if (StartGdiplus()) {
            Graphics graphics(dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            GraphicsPath hintPath;
            BuildRoundedRectPath(&hintPath, static_cast<float>(hintBg.left - offsetX),
                                 static_cast<float>(hintBg.top - offsetY),
                                 static_cast<float>(RectWidth(hintBg)),
                                 static_cast<float>(RectHeight(hintBg)), 10.0f);
            SolidBrush pillFill(Color(220, 16, 21, 24));
            Pen pillBorder(Color(170, 64, 80, 86), 1.0f);
            graphics.FillPath(&pillFill, &hintPath);
            graphics.DrawPath(&pillBorder, &hintPath);
            const wchar_t *hint = state->kind == CaptureLongRegion
                                      ? L"Drag the scrollable area, then release to start auto capture"
                                      : L"Drag any region. Press Tab to switch to long capture.";
            Font hintFont(L"Segoe UI", 12.0f, FontStyleRegular, UnitPixel);
            SolidBrush hintBrush(Color(255, 230, 240, 240));
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            RectF hintRect(static_cast<float>(hintBg.left - offsetX),
                           static_cast<float>(hintBg.top - offsetY),
                           static_cast<float>(RectWidth(hintBg)),
                           static_cast<float>(RectHeight(hintBg)));
            graphics.DrawString(hint, -1, &hintFont, hintRect, &fmt, &hintBrush);
        } else {
            FillRectColor(dc, {hintBg.left - offsetX, hintBg.top - offsetY,
                               hintBg.right - offsetX, hintBg.bottom - offsetY},
                          RGB(16, 21, 24));
        }
        DrawOverlayModeButton(dc, OverlayModeRect(cw, ch, 0), L"Region",
                              state->kind == CaptureRegion, offsetX, offsetY);
        DrawOverlayModeButton(dc, OverlayModeRect(cw, ch, 1), L"Long capture",
                              state->kind == CaptureLongRegion, offsetX, offsetY);
    }

    if (state->dragging) {
        RECT local = NormalizeRectPoints(state->start, state->current);
        local = ClampRect(local, 0, 0, state->capture.w, state->capture.h);
        if (RectWidth(local) > 0 && RectHeight(local) > 0) {
            BitBlt(dc, local.left - offsetX, local.top - offsetY, RectWidth(local), RectHeight(local), source,
                   local.left, local.top, SRCCOPY);
            if (StartGdiplus()) {
                Graphics graphics(dc);
                graphics.SetSmoothingMode(SmoothingModeAntiAlias);
                Pen accent(Color(255, 23, 198, 163), 2.4f);
                Pen white(Color(220, 245, 251, 248), 1.2f);
                int drawLeft = local.left - offsetX;
                int drawTop = local.top - offsetY;
                graphics.DrawRectangle(&accent, drawLeft, drawTop, RectWidth(local), RectHeight(local));
                int handle = 13;
                SolidBrush handleBrush(Color(255, 245, 251, 248));
                graphics.FillEllipse(&handleBrush, drawLeft - handle / 2, drawTop - handle / 2, handle, handle);
                graphics.FillEllipse(&handleBrush, local.right - offsetX - handle / 2, drawTop - handle / 2, handle, handle);
                graphics.FillEllipse(&handleBrush, drawLeft - handle / 2, local.bottom - offsetY - handle / 2, handle, handle);
                graphics.FillEllipse(&handleBrush, local.right - offsetX - handle / 2,
                                     local.bottom - offsetY - handle / 2, handle, handle);
                graphics.DrawRectangle(&white, drawLeft + 1, drawTop + 1,
                                       std::max(0, RectWidth(local) - 2), std::max(0, RectHeight(local) - 2));
            }

            wchar_t label[64];
            wsprintfW(label, L"%d x %d", RectWidth(local), RectHeight(local));
            RECT pill = {local.left, local.top - 28, local.left + 92, local.top - 4};
            if (pill.top < 8) {
                OffsetRect(&pill, 0, 36);
            }
            if (StartGdiplus()) {
                Graphics graphics(dc);
                graphics.SetSmoothingMode(SmoothingModeAntiAlias);
                GraphicsPath path;
                BuildRoundedRectPath(&path, static_cast<float>(pill.left - offsetX),
                                     static_cast<float>(pill.top - offsetY),
                                     static_cast<float>(RectWidth(pill)),
                                     static_cast<float>(RectHeight(pill)), 7.0f);
                SolidBrush brush(Color(236, 16, 21, 24));
                graphics.FillPath(&brush, &path);
            } else {
                RECT drawPill = {pill.left - offsetX, pill.top - offsetY,
                                 pill.right - offsetX, pill.bottom - offsetY};
                FillRectColor(dc, drawPill, RGB(16, 21, 24));
            }
            DrawTextLabel(dc, label, pill.left - offsetX + 10, pill.top - offsetY + 4, RGB(245, 251, 248));
        }
    }

    SelectObject(source, old);
    DeleteDC(source);
    (void)hwnd;
}

static void DrawOverlay(OverlayState *state, HWND hwnd, HDC dc) {
    DrawOverlayOffset(state, hwnd, dc, 0, 0);
}

static RECT OverlayVisualBounds(OverlayState *state) {
    RECT empty = {0, 0, 0, 0};
    if (!state || !state->dragging) {
        return empty;
    }

    RECT local = NormalizeRectPoints(state->start, state->current);
    local = ClampRect(local, 0, 0, state->capture.w, state->capture.h);
    if (RectWidth(local) <= 0 || RectHeight(local) <= 0) {
        return empty;
    }

    RECT bounds = ExpandedRect(local, 20, state->capture.w, state->capture.h);
    RECT label = {local.left, local.top - 28, local.left + 92, local.top - 4};
    if (label.top < 8) {
        OffsetRect(&label, 0, 36);
    }
    label = ClampRect(label, 0, 0, state->capture.w, state->capture.h);
    UnionRect(&bounds, &bounds, &label);
    return ExpandedRect(bounds, 2, state->capture.w, state->capture.h);
}

static void InvalidateOverlayChange(HWND hwnd, OverlayState *state, const RECT &oldBounds) {
    RECT next = OverlayVisualBounds(state);
    if (IsRectEmptyOrInvalid(oldBounds) && IsRectEmptyOrInvalid(next)) {
        return;
    }

    RECT dirty = {};
    if (IsRectEmptyOrInvalid(oldBounds)) {
        dirty = next;
    } else if (IsRectEmptyOrInvalid(next)) {
        dirty = oldBounds;
    } else {
        UnionRect(&dirty, &oldBounds, &next);
    }
    InvalidateRect(hwnd, &dirty, FALSE);
}

static void PaintOverlayBuffered(OverlayState *state, HWND hwnd, HDC dc, const RECT &paint) {
    int w = RectWidth(paint);
    int h = RectHeight(paint);
    if (!state || w <= 0 || h <= 0) {
        return;
    }

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP buffer = CreateCompatibleBitmap(dc, w, h);
    if (!memory || !buffer) {
        if (buffer) {
            DeleteObject(buffer);
        }
        if (memory) {
            DeleteDC(memory);
        }
        DrawOverlay(state, hwnd, dc);
        return;
    }

    HGDIOBJ old = SelectObject(memory, buffer);
    DrawOverlayOffset(state, hwnd, memory, paint.left, paint.top);
    BitBlt(dc, paint.left, paint.top, w, h, memory, 0, 0, SRCCOPY);
    SelectObject(memory, old);
    DeleteObject(buffer);
    DeleteDC(memory);
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
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_CROSS));
        return TRUE;
    case WM_KEYDOWN:
        if (!state) {
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            state->cancelled = true;
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == VK_TAB && state->showModeButtons) {
            state->kind = state->kind == CaptureRegion ? CaptureLongRegion : CaptureRegion;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (state) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (state->showModeButtons) {
                bool hitMode = false;
                CaptureKind mode = OverlayModeAt(state->capture.w, state->capture.h, x, y, &hitMode);
                if (hitMode) {
                    state->kind = mode;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            SetCapture(hwnd);
            state->dragging = true;
            state->start = {x, y};
            state->current = state->start;
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state && state->dragging) {
            RECT oldBounds = OverlayVisualBounds(state);
            state->current = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            InvalidateOverlayChange(hwnd, state, oldBounds);
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
            PaintOverlayBuffered(state, hwnd, dc, ps.rcPaint);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static bool RunSelectionOverlay(const ScreenCapture &capture, RECT *selection, CaptureKind *kind,
                                bool showModeButtons, CaptureKind initialKind) {
    OverlayState state;
    state.capture = capture;
    state.showModeButtons = showModeButtons;
    state.kind = initialKind;

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kOverlayClass, L"Modern Screenshot",
                                WS_POPUP, capture.x, capture.y, capture.w, capture.h,
                                NULL, NULL, g_instance, &state);
    if (!hwnd) {
        return false;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
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
    if (kind) {
        *kind = state.kind;
    }
    return true;
}

static void ClearPendingLongFrame(LongSessionState *state) {
    if (!state) {
        return;
    }
    if (state->pendingBitmap) {
        DeleteObject(state->pendingBitmap);
        state->pendingBitmap = NULL;
    }
    state->pendingPixels.clear();
    state->pendingStableFrames = 0;
}

static bool AppendLongFrameStrip(LongSessionState *state, HBITMAP bitmap, int overlap) {
    if (!state || !bitmap) {
        return false;
    }
    if (state->totalH >= kLongCaptureMaxHeight ||
        static_cast<int>(state->frames.size()) >= kLongCaptureMaxFrames) {
        state->captureLimitReached = true;
        return false;
    }

    int copyH = state->h - overlap;
    if (state->totalH + copyH > kLongCaptureMaxHeight) {
        copyH = kLongCaptureMaxHeight - state->totalH;
    }
    if (copyH <= 0) {
        return false;
    }

    HBITMAP strip = CopyBitmapStrip(bitmap, state->w, overlap, copyH);
    if (!strip) {
        return false;
    }
    state->frames.push_back({strip, copyH});
    state->totalH += copyH;
    state->lastCopyH = copyH;
    state->skippedFrames = 0;
    return true;
}

static bool AppendPendingLongTail(LongSessionState *state) {
    if (!state || !state->pendingBitmap || state->pendingPixels.empty()) {
        return false;
    }
    int overlapError = 255 * 3;
    int overlap = EstimateOverlap(state->prevPixels, state->pendingPixels, state->w, state->h, &overlapError);
    if (overlap <= 0 || overlap >= state->h) {
        return false;
    }

    HBITMAP bitmap = state->pendingBitmap;
    state->pendingBitmap = NULL;
    bool appended = AppendLongFrameStrip(state, bitmap, overlap);
    DeleteObject(bitmap);
    if (!appended) {
        state->pendingPixels.clear();
        state->pendingStableFrames = 0;
        return false;
    }
    state->prevPixels.swap(state->pendingPixels);
    state->pendingPixels.clear();
    state->pendingStableFrames = 0;
    return true;
}

static bool AddLongFrame(LongSessionState *state, HBITMAP bitmap, std::vector<BYTE> *pixels) {
    if (!state || !bitmap || !pixels) {
        return false;
    }
    state->lastSampleUnchanged = false;

    if (!state->hasPrev) {
        state->frames.push_back({bitmap, state->h});
        state->prevPixels.swap(*pixels);
        state->totalH = state->h;
        state->lastCopyH = state->h;
        state->hasPrev = true;
        ClearPendingLongFrame(state);
        return true;
    }

    if (IsSameFramePixels(state->prevPixels, *pixels)) {
        state->lastSampleUnchanged = true;
        DeleteObject(bitmap);
        return false;
    }

    int overlapError = 255 * 3;
    int overlap = EstimateOverlap(state->prevPixels, *pixels, state->w, state->h, &overlapError);
    if (overlap <= 0) {
        if (AverageFrameDiff(state->prevPixels, *pixels, state->w, state->h) <= 1) {
            state->lastSampleUnchanged = true;
        } else if (state->pendingBitmap && AverageFrameDiff(state->pendingPixels, *pixels, state->w, state->h) <= 2) {
            ++state->pendingStableFrames;
            DeleteObject(state->pendingBitmap);
            state->pendingBitmap = bitmap;
            state->pendingPixels.swap(*pixels);
            return false;
        } else {
            ClearPendingLongFrame(state);
            state->pendingBitmap = bitmap;
            state->pendingPixels.swap(*pixels);
            state->pendingStableFrames = 1;
            ++state->skippedFrames;
            return false;
        }
        ++state->skippedFrames;
        DeleteObject(bitmap);
        return false;
    }

    bool appended = AppendLongFrameStrip(state, bitmap, overlap);
    DeleteObject(bitmap);
    if (!appended) {
        return false;
    }
    state->prevPixels.swap(*pixels);
    ClearPendingLongFrame(state);
    return true;
}

static void PumpMessagesFor(HWND hwnd, int milliseconds);

static bool CaptureLongSelectionPixels(LongSessionState *state, HBITMAP *outBitmap, std::vector<BYTE> *outPixels) {
    if (!state || !outBitmap || !outPixels) {
        return false;
    }
    HBITMAP bitmap = CaptureScreenRect(state->selection);
    std::vector<BYTE> pixels;
    if (!bitmap || !ReadBitmapPixels(bitmap, state->w, state->h, &pixels)) {
        if (bitmap) {
            DeleteObject(bitmap);
        }
        return false;
    }
    *outBitmap = bitmap;
    outPixels->swap(pixels);
    return true;
}

static bool SampleLongSelection(LongSessionState *state) {
    HBITMAP bitmap = NULL;
    std::vector<BYTE> pixels;
    if (!CaptureLongSelectionPixels(state, &bitmap, &pixels)) {
        return false;
    }
    return AddLongFrame(state, bitmap, &pixels);
}

static bool SampleStableLongSelection(LongSessionState *state, HWND hwnd) {
    HBITMAP currentBitmap = NULL;
    std::vector<BYTE> currentPixels;
    if (!CaptureLongSelectionPixels(state, &currentBitmap, &currentPixels)) {
        return false;
    }

    for (int attempt = 0; attempt < kLongScrollSettleAttempts && IsWindow(hwnd); ++attempt) {
        PumpMessagesFor(hwnd, kLongScrollSettleMs);
        if (state->done || state->cancelled || !IsWindow(hwnd)) {
            DeleteObject(currentBitmap);
            return false;
        }

        HBITMAP nextBitmap = NULL;
        std::vector<BYTE> nextPixels;
        if (!CaptureLongSelectionPixels(state, &nextBitmap, &nextPixels)) {
            DeleteObject(currentBitmap);
            return false;
        }

        int diff = AverageFrameDiff(currentPixels, nextPixels, state->w, state->h);
        DeleteObject(currentBitmap);
        currentBitmap = nextBitmap;
        currentPixels.swap(nextPixels);
        if (diff <= 1) {
            bool added = AddLongFrame(state, currentBitmap, &currentPixels);
            return added;
        }
    }

    bool added = AddLongFrame(state, currentBitmap, &currentPixels);
    return added;
}

static bool IsOwnWindow(HWND hwnd) {
    while (hwnd) {
        wchar_t className[96] = {};
        GetClassNameW(hwnd, className, 96);
        if (wcscmp(className, kHiddenClass) == 0 || wcscmp(className, kOverlayClass) == 0 ||
            wcscmp(className, kLongSessionClass) == 0 || wcscmp(className, kEditorClass) == 0 ||
            wcscmp(className, kSettingsClass) == 0) {
            return true;
        }
        hwnd = GetParent(hwnd);
    }
    return false;
}

static HWND WindowAtPointForScroll(POINT point, HWND fallback) {
    HWND hwnd = WindowFromPoint(point);
    if (hwnd && !IsOwnWindow(hwnd)) {
        return hwnd;
    }
    return fallback;
}

static void FocusLongTarget(LongSessionState *state) {
    if (!state || !state->targetRoot || !IsWindow(state->targetRoot)) {
        return;
    }
    if (IsIconic(state->targetRoot)) {
        ShowWindow(state->targetRoot, SW_RESTORE);
    }
    SetForegroundWindow(state->targetRoot);
}

static bool SendLongScrollStep(LongSessionState *state) {
    if (!state || !state->target || !IsWindow(state->target)) {
        return false;
    }
    POINT center = {state->selection.left + state->w / 2, state->selection.top + state->h / 2};
    FocusLongTarget(state);
    SetCursorPos(center.x, center.y);
    HWND pointTarget = WindowAtPointForScroll(center, state->target);
    if (pointTarget && IsWindow(pointTarget)) {
        state->target = pointTarget;
        HWND root = RootWindow(pointTarget);
        if (root && !IsOwnWindow(root)) {
            state->targetRoot = root;
        }
    }
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(-WHEEL_DELTA);
    return SendInput(1, &input, sizeof(input)) == 1;
}

static void PumpMessagesFor(HWND hwnd, int milliseconds) {
    DWORD end = GetTickCount() + static_cast<DWORD>(milliseconds);
    MSG msg;
    do {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(10);
    } while (GetTickCount() < end && IsWindow(hwnd));
}

static bool CandidateLongPanelPosition(const RECT &selection, int x, int y, int *outX, int *outY) {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    x = Clamp(x, vx + 8, vx + vw - kLongSessionW - 8);
    y = Clamp(y, vy + 8, vy + vh - kLongSessionH - 8);

    RECT panel = {x, y, x + kLongSessionW, y + kLongSessionH};
    if (RectIntersects(panel, selection)) {
        return false;
    }
    *outX = x;
    *outY = y;
    return true;
}

static void LongPanelPosition(const RECT &selection, int *outX, int *outY) {
    const int gap = 16;
    int candidates[][2] = {
        {selection.right + gap, selection.top},
        {selection.left - kLongSessionW - gap, selection.top},
        {selection.right + gap, selection.bottom - kLongSessionH},
        {selection.left - kLongSessionW - gap, selection.bottom - kLongSessionH},
        {selection.left, selection.bottom + gap},
        {selection.left, selection.top - kLongSessionH - gap},
    };

    for (const auto &candidate : candidates) {
        if (CandidateLongPanelPosition(selection, candidate[0], candidate[1], outX, outY)) {
            return;
        }
    }

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    *outX = Clamp(selection.right + gap, vx + 8, vx + vw - kLongSessionW - 8);
    *outY = Clamp(selection.top, vy + 8, vy + vh - kLongSessionH - 8);
}

static RECT LongDoneRect() {
    return {176, 470, 256, 504};
}

static RECT LongCancelRect() {
    return {266, 470, 346, 504};
}

static void DrawLongPreview(LongSessionState *state, HDC dc, const RECT &preview) {
    FillRectColor(dc, preview, RGB(7, 11, 14));
    if (!state || state->frames.empty() || state->w <= 0 || state->totalH <= 0) {
        DrawCenteredText(dc, L"Preview will appear after the first frame", preview, -13, FW_MEDIUM, RGB(112, 130, 132));
        return;
    }

    int padding = 8;
    RECT content = {preview.left + padding, preview.top + padding, preview.right - padding, preview.bottom - padding};
    int contentW = RectWidth(content);
    int contentH = RectHeight(content);
    if (contentW <= 0 || contentH <= 0) {
        return;
    }

    int drawW = contentW;
    int totalScaledH = std::max(1, state->totalH * drawW / state->w);
    int visibleTop = std::max(0, totalScaledH - contentH);
    int drawX = content.left;

    HDC source = CreateCompatibleDC(dc);
    if (!source) {
        return;
    }

    int scaledY = 0;
    for (const LongFrame &frame : state->frames) {
        int frameScaledH = std::max(1, frame.copyH * drawW / state->w);
        int destTop = content.top + scaledY - visibleTop;
        int destBottom = destTop + frameScaledH;
        if (destBottom > content.top && destTop < content.bottom) {
            int clippedTop = std::max(destTop, static_cast<int>(content.top));
            int clippedBottom = std::min(destBottom, static_cast<int>(content.bottom));
            int clippedH = clippedBottom - clippedTop;
            int srcY = std::max(0, (clippedTop - destTop) * frame.copyH / frameScaledH);
            int srcH = std::max(1, clippedH * frame.copyH / frameScaledH);
            HGDIOBJ oldSource = SelectObject(source, frame.bitmap);
            SetStretchBltMode(dc, HALFTONE);
            StretchBlt(dc, drawX, clippedTop, drawW, clippedH,
                       source, 0, srcY, state->w, srcH, SRCCOPY);
            SelectObject(source, oldSource);
        }
        scaledY += frameScaledH;
    }

    DeleteDC(source);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        Pen border(Color(210, 82, 98, 100), 1.0f);
        graphics.DrawRectangle(&border, preview.left, preview.top, RectWidth(preview) - 1, RectHeight(preview) - 1);
        SolidBrush fade(Color(150, 7, 11, 14));
        graphics.FillRectangle(&fade, preview.left + 1, preview.top + 1, RectWidth(preview) - 2, 26);
    }

    wchar_t label[96];
    wsprintfW(label, L"Live preview  %d x %d", state->w, state->totalH);
    DrawTextLabel(dc, label, preview.left + 12, preview.top + 7, RGB(245, 251, 248));
}

static void DrawLongSession(HWND hwnd, LongSessionState *state, HDC dc) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    FillRectColor(dc, rect, RGB(16, 21, 24));

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush panel(Color(245, 16, 21, 24));
        graphics.FillRectangle(&panel, 0, 0, RectWidth(rect), RectHeight(rect));
        Pen border(Color(160, 23, 198, 163), 1.2f);
        graphics.DrawRectangle(&border, 0, 0, RectWidth(rect) - 1, RectHeight(rect) - 1);
    }

    wchar_t status[128];
    wsprintfW(status, L"Long capture  %d frames  %d px", static_cast<int>(state->frames.size()), state->totalH);
    DrawTextLabel(dc, status, 14, 12, RGB(245, 251, 248));
    if (!state->started) {
        DrawTextLabel(dc, L"Preparing automatic scroll capture...", 14, 34, RGB(188, 202, 200));
    } else if (state->captureLimitReached) {
        DrawTextLabel(dc, L"Finished or limit reached. Opening editor.", 14, 34, RGB(255, 207, 90));
    } else if (state->skippedFrames > 0) {
        DrawTextLabel(dc, L"Waiting for stable overlap...", 14, 34, RGB(255, 207, 90));
    } else if (!state->statusText.empty()) {
        DrawTextLabel(dc, state->statusText, 14, 34, RGB(188, 202, 200));
    } else {
        wchar_t hint[128];
        wsprintfW(hint, L"Auto scrolling. Last +%d px", state->lastCopyH);
        DrawTextLabel(dc, hint, 14, 34, RGB(188, 202, 200));
    }

    RECT preview = {14, 62, 346, 458};
    DrawLongPreview(state, dc, preview);

    RECT done = LongDoneRect();
    RECT cancel = LongCancelRect();
    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        GraphicsPath donePath;
        BuildRoundedRectPath(&donePath, static_cast<float>(done.left), static_cast<float>(done.top),
                             static_cast<float>(RectWidth(done)), static_cast<float>(RectHeight(done)), 9.0f);
        SolidBrush doneFill(Color(255, 23, 198, 163));
        graphics.FillPath(&doneFill, &donePath);
        GraphicsPath cancelPath;
        BuildRoundedRectPath(&cancelPath, static_cast<float>(cancel.left), static_cast<float>(cancel.top),
                             static_cast<float>(RectWidth(cancel)), static_cast<float>(RectHeight(cancel)), 9.0f);
        SolidBrush cancelFill(Color(255, 36, 45, 48));
        graphics.FillPath(&cancelFill, &cancelPath);
    } else {
        FillRectColor(dc, done, RGB(23, 198, 163));
        FillRectColor(dc, cancel, RGB(36, 45, 48));
    }
    DrawCenteredText(dc, L"Stop", done, -14, FW_SEMIBOLD, RGB(7, 11, 14));
    DrawCenteredText(dc, L"Cancel", cancel, -14, FW_SEMIBOLD, RGB(245, 251, 248));
}

static void PaintLongSessionBuffered(HWND hwnd, LongSessionState *state, HDC dc, const RECT &paint) {
    int w = RectWidth(paint);
    int h = RectHeight(paint);
    if (!state || w <= 0 || h <= 0) {
        return;
    }

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP buffer = CreateCompatibleBitmap(dc, w, h);
    if (!memory || !buffer) {
        if (buffer) {
            DeleteObject(buffer);
        }
        if (memory) {
            DeleteDC(memory);
        }
        DrawLongSession(hwnd, state, dc);
        return;
    }

    HGDIOBJ old = SelectObject(memory, buffer);
    SetViewportOrgEx(memory, -paint.left, -paint.top, NULL);
    DrawLongSession(hwnd, state, memory);
    SetViewportOrgEx(memory, 0, 0, NULL);
    BitBlt(dc, paint.left, paint.top, w, h, memory, 0, 0, SRCCOPY);
    SelectObject(memory, old);
    DeleteObject(buffer);
    DeleteDC(memory);
}

static LRESULT CALLBACK LongSessionProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LongSessionState *state = reinterpret_cast<LongSessionState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        state = reinterpret_cast<LongSessionState *>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_COMMAND:
        if (LOWORD(wParam) == kLongSessionDone && state) {
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wParam) == kLongSessionCancel && state) {
            state->cancelled = true;
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN && state) {
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == VK_ESCAPE && state) {
            state->cancelled = true;
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (state) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            RECT done = LongDoneRect();
            RECT cancel = LongCancelRect();
            if (PointInRect(x, y, done)) {
                state->done = true;
                DestroyWindow(hwnd);
            } else if (PointInRect(x, y, cancel)) {
                state->cancelled = true;
                state->done = true;
                DestroyWindow(hwnd);
            } else {
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            PaintLongSessionBuffered(hwnd, state, dc, ps.rcPaint);
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
    case WM_DESTROY:
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static HBITMAP RunLongSession(const RECT &selection, HWND preferredTarget, int *outH) {
    LongSessionState state;
    state.selection = selection;
    state.w = RectWidth(selection);
    state.h = RectHeight(selection);
    state.totalH = 0;
    POINT center = {selection.left + state.w / 2, selection.top + state.h / 2};
    state.target = WindowAtPointForScroll(center, preferredTarget);
    state.targetRoot = RootWindow(state.target);
    if (!state.targetRoot && preferredTarget && IsWindow(preferredTarget)) {
        state.targetRoot = RootWindow(preferredTarget);
        state.target = preferredTarget;
    }

    if (state.w <= 16 || state.h <= 16 || !SampleLongSelection(&state)) {
        return NULL;
    }

    FocusLongTarget(&state);
    SetCursorPos(center.x, center.y);

    int x = 0;
    int y = 0;
    LongPanelPosition(selection, &x, &y);

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                kLongSessionClass, L"ModernScreenshot Long Capture",
                                WS_POPUP, x, y, kLongSessionW, kLongSessionH, NULL, NULL, g_instance, &state);
    if (!hwnd) {
        for (LongFrame &frame : state.frames) {
            DeleteObject(frame.bitmap);
        }
        ClearPendingLongFrame(&state);
        return NULL;
    }

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);
    FocusLongTarget(&state);
    SetCursorPos(center.x, center.y);

    state.started = true;
    state.statusText = L"Auto scrolling...";
    InvalidateRect(hwnd, NULL, FALSE);
    PumpMessagesFor(hwnd, 120);

    while (!state.done && !state.cancelled && IsWindow(hwnd)) {
        if (state.totalH >= kLongCaptureMaxHeight ||
            static_cast<int>(state.frames.size()) >= kLongCaptureMaxFrames) {
            state.captureLimitReached = true;
            state.statusText = L"Limit reached.";
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }

        if (!SendLongScrollStep(&state)) {
            state.statusText = L"Target did not accept scrolling.";
            InvalidateRect(hwnd, NULL, FALSE);
            PumpMessagesFor(hwnd, 400);
            break;
        }

        PumpMessagesFor(hwnd, kLongScrollWaitMs);
        if (state.done || state.cancelled || !IsWindow(hwnd)) {
            break;
        }

        bool added = SampleStableLongSelection(&state, hwnd);
        if (added) {
            state.unchangedSteps = 0;
            state.statusText = L"Auto scrolling...";
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (!state.lastSampleUnchanged) {
            state.unchangedSteps = 0;
            state.statusText = state.pendingStableFrames >= 2 ? L"At bottom or content changed. Verifying..." :
                                                                 L"Waiting for stable overlap...";
            InvalidateRect(hwnd, NULL, FALSE);
        } else {
            ++state.unchangedSteps;
            state.statusText = L"Checking for page bottom...";
            InvalidateRect(hwnd, NULL, FALSE);
            if (state.unchangedSteps >= 3) {
                PumpMessagesFor(hwnd, kLongScrollWaitMs * 2);
                if (state.done || state.cancelled || !IsWindow(hwnd)) {
                    break;
                }
                SendLongScrollStep(&state);
                PumpMessagesFor(hwnd, kLongScrollWaitMs);
                if (state.done || state.cancelled || !IsWindow(hwnd)) {
                    break;
                }
                if (SampleStableLongSelection(&state, hwnd)) {
                    state.unchangedSteps = 0;
                    state.statusText = L"Auto scrolling...";
                    InvalidateRect(hwnd, NULL, FALSE);
                    continue;
                }
                if (!state.lastSampleUnchanged) {
                    state.unchangedSteps = 0;
                    state.statusText = L"Waiting for stable overlap...";
                    InvalidateRect(hwnd, NULL, FALSE);
                    continue;
                }
                if (state.pendingStableFrames >= 2 && AppendPendingLongTail(&state)) {
                    state.unchangedSteps = 0;
                    state.statusText = L"Added final tail.";
                    InvalidateRect(hwnd, NULL, FALSE);
                    continue;
                }
                if (state.pendingStableFrames >= 1 && AppendPendingLongTail(&state)) {
                    state.unchangedSteps = 0;
                    state.statusText = L"Added final tail.";
                    InvalidateRect(hwnd, NULL, FALSE);
                    continue;
                }
                state.captureLimitReached = true;
                state.statusText = L"Bottom reached.";
                InvalidateRect(hwnd, NULL, FALSE);
                PumpMessagesFor(hwnd, 350);
                break;
            }
        }
    }

    if (IsWindow(hwnd)) {
        DestroyWindow(hwnd);
    }

    HBITMAP stitched = NULL;
    if (!state.cancelled) {
        stitched = StitchLongFrames(state.frames, state.w, outH);
    }
    for (LongFrame &frame : state.frames) {
        DeleteObject(frame.bitmap);
    }
    ClearPendingLongFrame(&state);
    return stitched;
}

static bool PointInImage(EditorState *state, int x, int y) {
    return x >= state->imageX && y >= state->imageY &&
           x < state->imageX + state->w && y < state->imageY + state->viewH;
}

static POINT ToImagePoint(EditorState *state, int x, int y) {
    POINT point;
    point.x = Clamp(x - state->imageX, 0, state->w - 1);
    point.y = Clamp(y - state->imageY + state->scrollY, 0, state->h - 1);
    return point;
}

static bool ScrollEditor(EditorState *state, HWND hwnd, int delta) {
    if (!state || state->h <= state->viewH) {
        return false;
    }

    int old = state->scrollY;
    int maxScroll = std::max(0, state->h - state->viewH);
    state->scrollY = Clamp(state->scrollY + delta, 0, maxScroll);
    if (state->scrollY != old) {
        InvalidateRect(hwnd, NULL, FALSE);
        return true;
    }
    return false;
}

static void DrawToolbarIcon(HDC dc, Action action, const RECT &button, bool active) {
    COLORREF color = active ? RGB(23, 198, 163) : RGB(238, 244, 242);
    float x = static_cast<float>(button.left);
    float y = static_cast<float>(button.top);
    float w = static_cast<float>(RectWidth(button));

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        Color iconColor(255, GetRValue(color), GetGValue(color), GetBValue(color));
        Color muted(170, GetRValue(color), GetGValue(color), GetBValue(color));
        Pen pen(iconColor, 2.2f);
        Pen thin(muted, 1.5f);
        SolidBrush brush(iconColor);
        float cx = x + w / 2.0f;
        float top = y + 9.0f;

        if (action == ActionToolBlur) {
            float s = 6.5f;
            float startX = cx - 8.0f;
            graphics.FillRectangle(&brush, startX, top + 2.0f, s, s);
            graphics.FillRectangle(&brush, startX + 10.0f, top + 2.0f, s, s);
            graphics.FillRectangle(&brush, startX, top + 12.0f, s, s);
            graphics.FillRectangle(&brush, startX + 10.0f, top + 12.0f, s, s);
        } else if (action == ActionToolArrow) {
            graphics.DrawLine(&pen, cx - 12.0f, top + 18.0f, cx + 11.0f, top + 5.0f);
            graphics.DrawLine(&pen, cx + 11.0f, top + 5.0f, cx + 3.0f, top + 5.0f);
            graphics.DrawLine(&pen, cx + 11.0f, top + 5.0f, cx + 9.0f, top + 13.0f);
        } else if (action == ActionToolRect) {
            graphics.DrawRectangle(&pen, cx - 12.0f, top + 4.0f, 24.0f, 16.0f);
        } else if (action == ActionUndo) {
            graphics.DrawArc(&pen, cx - 12.0f, top + 4.0f, 24.0f, 18.0f, 35.0f, 285.0f);
            graphics.DrawLine(&pen, cx - 12.0f, top + 11.0f, cx - 6.0f, top + 4.0f);
            graphics.DrawLine(&pen, cx - 12.0f, top + 11.0f, cx - 4.0f, top + 13.0f);
        } else if (action == ActionCopy) {
            graphics.DrawRectangle(&thin, cx - 7.0f, top + 3.0f, 16.0f, 15.0f);
            graphics.DrawRectangle(&pen, cx - 11.0f, top + 8.0f, 16.0f, 15.0f);
        } else if (action == ActionSave) {
            graphics.DrawLine(&pen, cx, top + 3.0f, cx, top + 17.0f);
            graphics.DrawLine(&pen, cx, top + 17.0f, cx - 6.0f, top + 11.0f);
            graphics.DrawLine(&pen, cx, top + 17.0f, cx + 6.0f, top + 11.0f);
            graphics.DrawLine(&pen, cx - 11.0f, top + 22.0f, cx + 11.0f, top + 22.0f);
        } else {
            Font font(L"Segoe UI", 23.0f, FontStyleBold, UnitPixel);
            graphics.DrawString(L"T", -1, &font, PointF(cx - 7.0f, top - 1.0f), &brush);
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

static void DrawToolbar(EditorState *state, HDC dc) {
    RECT toolbar = {0, 0, state->winW, kToolbarH};
    FillRectColor(dc, toolbar, RGB(12, 16, 18));

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush panel(Color(245, 18, 24, 27));
        graphics.FillRectangle(&panel, 0, 0, state->winW, kToolbarH);
        Pen line(Color(120, 80, 96, 98), 1.0f);
        graphics.DrawLine(&line, 0, kToolbarH - 1, state->winW, kToolbarH - 1);
    } else {
        HPEN border = CreatePen(PS_SOLID, 1, RGB(58, 74, 80));
        HGDIOBJ oldPen = SelectObject(dc, border);
        MoveToEx(dc, 0, kToolbarH - 1, NULL);
        LineTo(dc, state->winW, kToolbarH - 1);
        SelectObject(dc, oldPen);
        DeleteObject(border);
    }

    for (int i = 0; i < ButtonCount(); ++i) {
        RECT button = ButtonRect(i);
        bool isTool = kButtons[i].action == ActionToolBlur || kButtons[i].action == ActionToolArrow ||
                      kButtons[i].action == ActionToolRect || kButtons[i].action == ActionToolText;
        bool active = isTool && kButtons[i].tool == state->activeTool;
        if (StartGdiplus()) {
            Graphics graphics(dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            GraphicsPath path;
            BuildRoundedRectPath(&path, static_cast<float>(button.left), static_cast<float>(button.top),
                                 static_cast<float>(RectWidth(button)),
                                 static_cast<float>(RectHeight(button)), 8.0f);
            SolidBrush fill(active ? Color(255, 28, 48, 52) : Color(255, 21, 28, 31));
            Pen outline(active ? Color(255, 23, 198, 163) : Color(120, 82, 98, 100), active ? 1.6f : 1.0f);
            graphics.FillPath(&fill, &path);
            graphics.DrawPath(&outline, &path);
        } else {
            FillRectColor(dc, button, active ? RGB(33, 49, 56) : RGB(22, 29, 34));
            HPEN pen = CreatePen(PS_SOLID, 1, active ? RGB(23, 198, 163) : RGB(58, 74, 80));
            HGDIOBJ old = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, button.left, button.top, button.right, button.bottom);
            SelectObject(dc, oldBrush);
            SelectObject(dc, old);
            DeleteObject(pen);
        }

        DrawToolbarIcon(dc, kButtons[i].action, button, active);
        RECT labelRect = {button.left, button.bottom - 15, button.right, button.bottom - 2};
        DrawCenteredText(dc, kButtons[i].label, labelRect, -10, FW_MEDIUM,
                         active ? RGB(23, 198, 163) : RGB(188, 202, 200));
    }

    if (!state->statusText.empty() && state->winW > ToolbarWidth() + 120) {
        DrawTextLabel(dc, state->statusText, ToolbarWidth() + 8, 22, RGB(188, 202, 200));
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

static bool SaveEditor(EditorState *state, std::wstring *savedPath) {
    HBITMAP output = ComposeEditorBitmap(state);
    if (!output) {
        return false;
    }
    std::wstring path = BuildScreenshotPath();
    bool ok = SaveHbitmapPng(output, path);
    if (ok && savedPath) {
        *savedPath = path;
    }
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
    FillRectColor(dc, bg, RGB(12, 16, 18));

    HDC source = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(source, state->shot);
    int visibleH = std::min(state->viewH, state->h - state->scrollY);
    BitBlt(dc, state->imageX, state->imageY, state->w, visibleH, source, 0, state->scrollY, SRCCOPY);
    SelectObject(source, old);
    DeleteDC(source);

    RECT imageBorder = {state->imageX, state->imageY, state->imageX + state->w, state->imageY + state->viewH};
    HPEN border = CreatePen(PS_SOLID, 1, RGB(58, 74, 80));
    HGDIOBJ oldPen = SelectObject(dc, border);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, imageBorder.left, imageBorder.top, imageBorder.right, imageBorder.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(border);

    int savedClip = SaveDC(dc);
    IntersectClipRect(dc, state->imageX, state->imageY, state->imageX + state->w, state->imageY + state->viewH);
    for (const Annotation &op : state->ops) {
        DrawAnnotation(dc, op, state->imageX, state->imageY - state->scrollY, state->w, state->h);
    }
    if (state->drawing) {
        DrawAnnotation(dc, state->preview, state->imageX, state->imageY - state->scrollY, state->w, state->h);
    }
    if (state->textEntry) {
        Annotation text;
        text.tool = ToolText;
        text.a = state->textPoint;
        text.b = state->textPoint;
        text.text = state->textBuffer + L"_";
        DrawAnnotation(dc, text, state->imageX, state->imageY - state->scrollY, state->w, state->h);
    }
    RestoreDC(dc, savedClip);

    if (state->h > state->viewH && StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        int trackX = state->imageX + state->w - 8;
        int trackTop = state->imageY + 8;
        int trackH = std::max(24, state->viewH - 16);
        float thumbH = std::max(24.0f, static_cast<float>(trackH) * static_cast<float>(state->viewH) / static_cast<float>(state->h));
        float maxThumbTravel = static_cast<float>(trackH) - thumbH;
        float thumbY = static_cast<float>(trackTop);
        if (state->h > state->viewH) {
            thumbY += maxThumbTravel * static_cast<float>(state->scrollY) / static_cast<float>(state->h - state->viewH);
        }
        SolidBrush track(Color(110, 8, 12, 14));
        SolidBrush thumb(Color(210, 23, 198, 163));
        graphics.FillRectangle(&track, static_cast<float>(trackX), static_cast<float>(trackTop), 4.0f, static_cast<float>(trackH));
        graphics.FillRectangle(&thumb, static_cast<float>(trackX - 1), thumbY, 6.0f, thumbH);
    }

    DrawToolbar(state, dc);
    (void)hwnd;
}

static void PaintEditorBuffered(EditorState *state, HWND hwnd, HDC dc, const RECT &paint) {
    int w = RectWidth(paint);
    int h = RectHeight(paint);
    if (!state || w <= 0 || h <= 0) {
        return;
    }

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP buffer = CreateCompatibleBitmap(dc, w, h);
    if (!memory || !buffer) {
        if (buffer) {
            DeleteObject(buffer);
        }
        if (memory) {
            DeleteDC(memory);
        }
        DrawEditor(state, hwnd, dc);
        return;
    }

    HGDIOBJ old = SelectObject(memory, buffer);
    int saved = SaveDC(memory);
    SetViewportOrgEx(memory, -paint.left, -paint.top, NULL);
    DrawEditor(state, hwnd, memory);
    RestoreDC(memory, saved);
    BitBlt(dc, paint.left, paint.top, w, h, memory, 0, 0, SRCCOPY);
    SelectObject(memory, old);
    DeleteObject(buffer);
    DeleteDC(memory);
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

static void NotifyAction(HWND owner, const std::wstring &title, const std::wstring &message) {
    if (g_hiddenWindow) {
        ShowTrayBalloon(g_hiddenWindow, title, message);
    } else {
        MessageBoxW(owner, message.c_str(), title.c_str(), MB_OK);
    }
}

static void HandleAction(EditorState *state, HWND hwnd, Action action, Tool tool) {
    if (action == ActionToolBlur || action == ActionToolArrow || action == ActionToolRect || action == ActionToolText) {
        state->activeTool = tool;
        state->textEntry = false;
        state->statusText.clear();
    } else if (action == ActionUndo) {
        if (!state->ops.empty()) {
            state->ops.pop_back();
        }
        state->textEntry = false;
        state->statusText = L"Undo";
    } else if (action == ActionSave) {
        std::wstring path;
        if (SaveEditor(state, &path)) {
            NotifyAction(hwnd, L"Screenshot saved", path);
            state->done = true;
            DestroyWindow(hwnd);
        } else {
            state->statusText = L"Save failed";
            NotifyAction(hwnd, L"Save failed", L"Could not write the screenshot file.");
        }
    } else if (action == ActionCopy) {
        if (CopyEditor(state, hwnd)) {
            NotifyAction(hwnd, L"Screenshot copied", L"Image copied to clipboard.");
            state->done = true;
            DestroyWindow(hwnd);
        } else {
            state->statusText = L"Copy failed";
            NotifyAction(hwnd, L"Copy failed", L"Could not write the image to clipboard.");
        }
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
    case WM_ERASEBKGND:
        return 1;
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
        if (wParam == VK_NEXT) {
            ScrollEditor(state, hwnd, std::max(80, state->viewH - 80));
            return 0;
        }
        if (wParam == VK_PRIOR) {
            ScrollEditor(state, hwnd, -std::max(80, state->viewH - 80));
            return 0;
        }
        if (wParam == VK_HOME) {
            ScrollEditor(state, hwnd, -state->h);
            return 0;
        }
        if (wParam == VK_END) {
            ScrollEditor(state, hwnd, state->h);
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
            if (y < kToolbarH) {
                ReleaseCapture();
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
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
    case WM_MOUSEWHEEL:
        if (state) {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (ScrollEditor(state, hwnd, delta > 0 ? -240 : 240)) {
                return 0;
            }
        }
        break;
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
            PaintEditorBuffered(state, hwnd, dc, ps.rcPaint);
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

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int maxWinH = std::max(kToolbarH + 160, vh - kEditorMargin);
    state.viewH = std::min(h, maxWinH - kToolbarH);
    state.winW = std::max(w, ToolbarWidth());
    state.winH = state.viewH + kToolbarH;
    state.imageX = (state.winW - w) / 2;
    state.imageY = kToolbarH;
    int x = vx + std::max(0, (vw - state.winW) / 2);
    int y = vy + std::max(0, (vh - state.winH) / 2);

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kEditorClass, L"ModernScreenshot",
                                WS_POPUP, x, y, state.winW, state.winH,
                                NULL, NULL, g_instance, &state);
    if (!hwnd) {
        return;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    MSG msg;
    while (!state.done && GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

static void RunSelectedCapture(CaptureKind initialKind) {
    if (g_captureActive) {
        return;
    }
    g_captureActive = true;
    HWND previousForeground = GetForegroundWindow();
    ScreenCapture capture = CaptureVirtualScreen();
    if (!capture.bitmap) {
        g_captureActive = false;
        return;
    }

    RECT selection;
    CaptureKind kind = initialKind;
    if (RunSelectionOverlay(capture, &selection, &kind, true, initialKind)) {
        if (kind == CaptureLongRegion) {
            DeleteObject(capture.bitmap);
            capture.bitmap = NULL;
            Sleep(120);

            int longH = 0;
            HBITMAP shot = RunLongSession(selection, previousForeground, &longH);
            if (shot) {
                RunEditorModal(shot, RectWidth(selection), longH);
                DeleteObject(shot);
            } else {
                NotifyAction(g_hiddenWindow, L"Long capture cancelled",
                             L"No long screenshot was created.");
            }
        } else {
            HBITMAP shot = CropCapture(capture, selection);
            if (shot) {
                RunEditorModal(shot, RectWidth(selection), RectHeight(selection));
                DeleteObject(shot);
            }
        }
    }
    if (capture.bitmap) {
        DeleteObject(capture.bitmap);
    }
    g_captureActive = false;
}

static void RunCapture() {
    RunSelectedCapture(CaptureRegion);
}

static void RunLongCapture() {
    RunSelectedCapture(CaptureLongRegion);
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

static HICON CreateTrayIcon() {
    HDC screen = GetDC(NULL);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP color = CreateCompatibleBitmap(screen, 32, 32);
    std::vector<BYTE> maskBits(32 * 32 / 8, 0);
    HBITMAP mask = CreateBitmap(32, 32, 1, 1, maskBits.data());
    HGDIOBJ old = SelectObject(dc, color);

    RECT bg = {0, 0, 32, 32};
    FillRectColor(dc, bg, RGB(16, 20, 24));
    HBRUSH accent = CreateSolidBrush(RGB(23, 198, 163));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, accent));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(245, 251, 248));
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

static void ShowTrayBalloon(HWND hwnd, const std::wstring &title, const std::wstring &message) {
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

static bool RegisterConfiguredHotkey(HWND hwnd, bool notify) {
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

enum SettingsHit {
    SettingsHitNone = 0,
    SettingsHitHotkey = 1,
    SettingsHitStartup = 2,
    SettingsHitApply = 3,
    SettingsHitCancel = 4,
};

enum SettingsBanner {
    BannerNone = 0,
    BannerInfo = 1,
    BannerWarning = 2,
    BannerSuccess = 3,
};

struct SettingsViewState {
    Settings pending;
    bool startup = false;
    bool capturing = false;
    UINT captureModifiers = 0;
    SettingsHit hover = SettingsHitNone;
    SettingsHit pressed = SettingsHitNone;
    std::wstring status;
    SettingsBanner banner = BannerNone;
};

static const int kSettingsClientW = 480;
static const int kSettingsClientH = 420;
static const int kSettingsPadX = 28;

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
    SetSettingsBanner(state, BannerInfo,
                      L"Press the keys you want to use. Esc to cancel.");
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
    Settings prev = g_settings;
    g_settings = state->pending;
    if (!RegisterConfiguredHotkey(g_hiddenWindow, false)) {
        g_settings = prev;
        RegisterConfiguredHotkey(g_hiddenWindow, false);
        SetSettingsBanner(state, BannerWarning,
                          L"Hotkey " + HotkeyText(state->pending) + L" is already used by another app.");
        InvalidateRect(hwnd, NULL, FALSE);
        return false;
    }
    SaveSettings();
    if (!SetStartupEnabled(state->startup)) {
        SetSettingsBanner(state, BannerWarning,
                          L"Saved hotkey, but could not update Windows startup.");
        InvalidateRect(hwnd, NULL, FALSE);
        return false;
    }
    ShowTrayBalloon(g_hiddenWindow, L"Settings updated", HotkeyText(g_settings));
    return true;
}

static LRESULT CALLBACK SettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    SettingsViewState *state = reinterpret_cast<SettingsViewState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        SettingsViewState *view = new SettingsViewState();
        view->pending = g_settings;
        view->startup = IsStartupEnabled();
        SetSettingsBanner(view, BannerInfo, L"Current hotkey: " + HotkeyText(g_settings));
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
        SetClassLongPtrW(hwnd, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(LoadCursor(NULL, IDC_ARROW)));
        if (g_trayIcon) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_trayIcon));
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_trayIcon));
        }
        (void)create;
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (!state) {
            return 0;
        }
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        SettingsHit hit = SettingsHitTest(x, y);
        state->pressed = hit;
        SetCapture(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!state) {
            return 0;
        }
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
        if (!state) {
            return 0;
        }
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
        if (!state) {
            return 0;
        }
        UINT vk = static_cast<UINT>(wParam);
        if (state->capturing) {
            if (vk == VK_ESCAPE) {
                CancelHotkeyCapture(state);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            UINT bit = ModifierBitForVk(vk);
            if (bit) {
                state->captureModifiers |= bit;
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
            }
            return 0;
        }
        return 0;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (state && state->capturing) {
            UINT vk = static_cast<UINT>(wParam);
            UINT bit = ModifierBitForVk(vk);
            if (bit) {
                state->captureModifiers &= ~bit;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            PaintSettingsWindow(hwnd, dc, *state);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    case WM_NCDESTROY:
        if (state) {
            delete state;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    case WM_DESTROY:
        if (g_settingsWindow == hwnd) {
            g_settingsWindow = NULL;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static void ShowSettingsWindow() {
    if (g_settingsWindow) {
        ShowWindow(g_settingsWindow, SW_SHOW);
        SetForegroundWindow(g_settingsWindow);
        return;
    }

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = 0;
    RECT desired = {0, 0, kSettingsClientW, kSettingsClientH};
    AdjustWindowRectEx(&desired, style, FALSE, exStyle);
    int winW = desired.right - desired.left;
    int winH = desired.bottom - desired.top;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = std::max(0, (screenW - winW) / 2);
    int y = std::max(0, (screenH - winH) / 2);

    g_settingsWindow = CreateWindowExW(exStyle, kSettingsClass, L"ModernScreenshot Settings",
                                       style, x, y, winW, winH,
                                       NULL, NULL, g_instance, NULL);
    if (g_settingsWindow) {
        ShowWindow(g_settingsWindow, SW_SHOW);
        SetForegroundWindow(g_settingsWindow);
    }
}

struct TrayPopupItem {
    UINT command;
    const wchar_t *label;
    const wchar_t *shortcut;
    const wchar_t *glyph;
    bool isSeparator;
};

struct TrayPopupState {
    int hoverIndex = -1;
    int itemCount = 0;
    TrayPopupItem items[8];
    int width = 0;
    int height = 0;
    bool dismissing = false;
};

static const int kTrayPopupRowH = 38;
static const int kTrayPopupSepH = 10;
static const int kTrayPopupHeaderH = 36;
static const int kTrayPopupPadX = 14;
static const int kTrayPopupRadius = 10;

static HWND g_trayPopup = NULL;

static int TrayPopupHitIndex(const TrayPopupState &state, int x, int y) {
    if (y < kTrayPopupHeaderH) {
        return -1;
    }
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
    SolidBrush fill(Color(255, 24, 30, 36));
    Pen border(Color(255, 60, 76, 84), 1.0f);
    graphics->FillPath(&fill, &path);
    graphics->DrawPath(&border, &path);
}

static void DrawTrayPopupHeader(Graphics *graphics, const TrayPopupState &state) {
    Font title(L"Segoe UI", 11.0f, FontStyleBold, UnitPixel);
    SolidBrush titleBrush(Color(255, 220, 232, 232));
    graphics->DrawString(L"ModernScreenshot", -1, &title,
                         PointF(static_cast<float>(kTrayPopupPadX), 10.0f), &titleBrush);

    Font ver(L"Segoe UI", 10.0f, FontStyleRegular, UnitPixel);
    SolidBrush verBrush(Color(255, 116, 138, 142));
    std::wstring vtext = L"v";
    vtext += kVersion;
    RectF vRect(static_cast<float>(state.width - kTrayPopupPadX - 80), 11.0f, 80.0f, 16.0f);
    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(Gdiplus::StringAlignmentFar);
    graphics->DrawString(vtext.c_str(), -1, &ver, vRect, &fmt, &verBrush);

    Pen rule(Color(255, 44, 56, 64), 1.0f);
    graphics->DrawLine(&rule, 12, kTrayPopupHeaderH - 1,
                       state.width - 12, kTrayPopupHeaderH - 1);
}

static void DrawTrayPopupRow(Graphics *graphics, const TrayPopupItem &item, const RECT &rect, bool hover) {
    if (item.isSeparator) {
        Pen line(Color(255, 44, 56, 64), 1.0f);
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
        SolidBrush fill(Color(255, 36, 48, 56));
        graphics->FillPath(&fill, &path);
    }

    float midY = (rect.top + rect.bottom) / 2.0f;

    Font glyph(L"Segoe UI", 15.0f, FontStyleRegular, UnitPixel);
    SolidBrush glyphBrush(hover ? Color(255, 23, 198, 163) : Color(255, 188, 210, 212));
    graphics->DrawString(item.glyph, -1, &glyph,
                         PointF(static_cast<float>(rect.left + kTrayPopupPadX), midY - 11.0f),
                         &glyphBrush);

    Font label(L"Segoe UI", 12.0f, FontStyleRegular, UnitPixel);
    SolidBrush labelBrush(Color(255, 232, 240, 242));
    graphics->DrawString(item.label, -1, &label,
                         PointF(static_cast<float>(rect.left + kTrayPopupPadX + 28), midY - 9.0f),
                         &labelBrush);

    if (item.command == kMenuCapture) {
        std::wstring keys = HotkeyText(g_settings);
        Font ks(L"Segoe UI", 11.0f, FontStyleRegular, UnitPixel);
        SolidBrush ksBrush(Color(255, 138, 158, 162));
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
    HBRUSH bg = CreateSolidBrush(RGB(24, 30, 36));
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
    if (index < 0 || index >= state.itemCount) {
        return;
    }
    const TrayPopupItem &item = state.items[index];
    if (item.isSeparator || item.command == 0) {
        return;
    }
    if (g_hiddenWindow) {
        PostMessageW(g_hiddenWindow, WM_COMMAND, MAKEWPARAM(item.command, 0), 0);
    }
}

static int FirstSelectableIndex(const TrayPopupState &state, int start, int direction) {
    if (state.itemCount == 0) {
        return -1;
    }
    int i = start;
    for (int n = 0; n < state.itemCount; ++n) {
        if (i < 0) i = state.itemCount - 1;
        if (i >= state.itemCount) i = 0;
        if (!state.items[i].isSeparator) {
            return i;
        }
        i += direction;
    }
    return -1;
}

static LRESULT CALLBACK TrayPopupProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
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

static void ShowTrayPopup(HWND hwnd) {
    if (g_trayPopup) {
        DestroyWindow(g_trayPopup);
        g_trayPopup = NULL;
    }

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
                                  hwnd, NULL, g_instance, NULL);
    if (!g_trayPopup) {
        return;
    }
    SetForegroundWindow(hwnd);
    ShowWindow(g_trayPopup, SW_SHOWNOACTIVATE);
    SetForegroundWindow(g_trayPopup);
    SetFocus(g_trayPopup);
}

static LRESULT CALLBACK HiddenProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (g_wakeupMessage && message == g_wakeupMessage) {
        ShowSettingsWindow();
        return 0;
    }
    switch (message) {
    case kTrayMessage:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            ShowTrayPopup(hwnd);
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
            ShowSettingsWindow();
        } else if (LOWORD(wParam) == kMenuExit) {
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        if (g_settingsWindow) {
            DestroyWindow(g_settingsWindow);
            g_settingsWindow = NULL;
        }
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
    overlay.hbrBackground = NULL;
    overlay.lpszClassName = kOverlayClass;
    RegisterClassW(&overlay);

    WNDCLASSW longSession = {};
    longSession.lpfnWndProc = LongSessionProc;
    longSession.hInstance = g_instance;
    longSession.hCursor = LoadCursor(NULL, IDC_ARROW);
    longSession.hbrBackground = NULL;
    longSession.lpszClassName = kLongSessionClass;
    RegisterClassW(&longSession);

    WNDCLASSW editor = {};
    editor.lpfnWndProc = EditorProc;
    editor.hInstance = g_instance;
    editor.hCursor = LoadCursor(NULL, IDC_ARROW);
    editor.hbrBackground = NULL;
    editor.lpszClassName = kEditorClass;
    RegisterClassW(&editor);

    WNDCLASSW settings = {};
    settings.lpfnWndProc = SettingsProc;
    settings.hInstance = g_instance;
    settings.hCursor = LoadCursor(NULL, IDC_ARROW);
    settings.hbrBackground = NULL;
    settings.lpszClassName = kSettingsClass;
    RegisterClassW(&settings);

    WNDCLASSW trayPopup = {};
    trayPopup.lpfnWndProc = TrayPopupProc;
    trayPopup.hInstance = g_instance;
    trayPopup.hCursor = LoadCursor(NULL, IDC_ARROW);
    trayPopup.hbrBackground = NULL;
    trayPopup.lpszClassName = kTrayPopupClass;
    trayPopup.style = CS_DROPSHADOW;
    RegisterClassW(&trayPopup);
}

static void UnregisterWindowClasses() {
    UnregisterClassW(kTrayPopupClass, g_instance);
    UnregisterClassW(kSettingsClass, g_instance);
    UnregisterClassW(kEditorClass, g_instance);
    UnregisterClassW(kLongSessionClass, g_instance);
    UnregisterClassW(kOverlayClass, g_instance);
    UnregisterClassW(kHiddenClass, g_instance);
}

static bool PrintVersionToConsole() {
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

static bool WakePreviousInstance() {
    HWND existing = FindWindowExW(HWND_MESSAGE, NULL, kHiddenClass, NULL);
    if (!existing) {
        return false;
    }
    if (!g_wakeupMessage) {
        g_wakeupMessage = RegisterWindowMessageW(kWakeupMessageName);
    }
    if (g_wakeupMessage) {
        PostMessageW(existing, g_wakeupMessage, 0, 0);
    }
    return true;
}

static int RunDaemon() {
    HWND hwnd = CreateWindowExW(0, kHiddenClass, L"ModernScreenshot", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, g_instance, NULL);
    if (!hwnd) {
        return 1;
    }
    g_hiddenWindow = hwnd;
    AddTrayIcon(hwnd);
    EnsureStartupPathSynced();

    if (!RegisterConfiguredHotkey(hwnd, true)) {
        ShowSettingsWindow();
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    g_instance = instance;
    SetProcessDPIAware();
    LoadSettings();

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int status = 0;

    if (argc > 1 && wcscmp(argv[1], L"--version") == 0) {
        PrintVersionToConsole();
        if (argv) {
            LocalFree(argv);
        }
        return 0;
    }

    g_wakeupMessage = RegisterWindowMessageW(kWakeupMessageName);

    if (argc == 1) {
        g_singleInstanceMutex = CreateMutexW(NULL, FALSE, kSingleInstanceName);
        if (g_singleInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
            WakePreviousInstance();
            CloseHandle(g_singleInstanceMutex);
            g_singleInstanceMutex = NULL;
            if (argv) {
                LocalFree(argv);
            }
            return 0;
        }
    }

    RegisterWindowClasses();

    if (argc > 2 && wcscmp(argv[1], L"--self-test") == 0) {
        status = SelfTestExport(argv[2]) ? 0 : 1;
    } else if (argc > 1 && wcscmp(argv[1], L"--once") == 0) {
        RunCapture();
        status = 0;
    } else if (argc > 1 && wcscmp(argv[1], L"--long") == 0) {
        RunLongCapture();
        status = 0;
    } else if (argc == 1) {
        status = RunDaemon();
    } else {
        MessageBoxW(NULL,
                    L"Usage:\nModernScreenshot.exe\nModernScreenshot.exe --once\nModernScreenshot.exe --long\nModernScreenshot.exe --self-test PATH\nModernScreenshot.exe --version",
                    L"ModernScreenshot", MB_OK);
        status = 2;
    }

    if (argv) {
        LocalFree(argv);
    }
    UnregisterWindowClasses();
    StopGdiplus();
    if (g_singleInstanceMutex) {
        CloseHandle(g_singleInstanceMutex);
        g_singleInstanceMutex = NULL;
    }
    return status;
}
