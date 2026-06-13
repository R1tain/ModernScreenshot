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
static const wchar_t *kLengthClass = L"ModernScreenshotLength";
static const wchar_t *kEditorClass = L"ModernScreenshotEditor";
static const wchar_t *kSettingsClass = L"ModernScreenshotSettings";
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
static const int kLongCaptureMaxFrames = 8;
static const int kLongCaptureMaxHeight = 20000;
static const int kLongCaptureWheelNotches = 7;
static const int kLongCaptureScrollDelayMs = 260;
static const int kSettingsApply = 3001;
static const int kSettingsCancel = 3002;
static const int kSettingsCtrl = 3003;
static const int kSettingsAlt = 3004;
static const int kSettingsShift = 3005;
static const int kSettingsWin = 3006;
static const int kSettingsKey = 3007;
static const int kSettingsStatus = 3008;
static const int kSettingsStartup = 3009;
static const int kOverlayModeW = 116;
static const int kOverlayModeH = 34;
static const int kOverlayModeGap = 8;
static const int kLengthMin = 120;

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
    int sourceY = 0;
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

struct LengthState {
    ScreenCapture capture;
    RECT selection = {0, 0, 0, 0};
    bool dragging = false;
    bool done = false;
    bool cancelled = false;
    int length = 0;
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

static void ShowTrayBalloon(HWND hwnd, const std::wstring &title, const std::wstring &message);
static HBITMAP CaptureLongSelection(const RECT &selection, int desiredHeight, int *outH);

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

static RECT OverlayModeRect(int index) {
    RECT rect;
    rect.left = 16 + index * (kOverlayModeW + kOverlayModeGap);
    rect.top = 16;
    rect.right = rect.left + kOverlayModeW;
    rect.bottom = rect.top + kOverlayModeH;
    return rect;
}

static bool PointInRect(int x, int y, const RECT &rect) {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

static CaptureKind OverlayModeAt(int x, int y, bool *hit) {
    RECT region = OverlayModeRect(0);
    if (PointInRect(x, y, region)) {
        *hit = true;
        return CaptureRegion;
    }

    RECT longRegion = OverlayModeRect(1);
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

static int OverlapError(const std::vector<BYTE> &prev, const std::vector<BYTE> &next, int w, int h, int overlap) {
    int xStep = std::max(1, w / 40);
    int yStep = std::max(1, overlap / 40);
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

static int EstimateOverlap(const std::vector<BYTE> &prev, const std::vector<BYTE> &next, int w, int h) {
    if (w <= 0 || h <= 1) {
        return 0;
    }

    int minOverlap = std::min(h - 1, std::max(24, h / 8));
    int maxOverlap = std::min(h - 1, std::max(minOverlap, h * 9 / 10));
    int step = std::max(1, h / 80);
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

    return bestError <= 45 ? bestOverlap : 0;
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
        BitBlt(dest, 0, y, w, frame.copyH, source, 0, frame.sourceY, SRCCOPY);
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
        DrawOverlayModeButton(dc, OverlayModeRect(0), L"Screenshot", state->kind == CaptureRegion, offsetX, offsetY);
        DrawOverlayModeButton(dc, OverlayModeRect(1), L"Long", state->kind == CaptureLongRegion, offsetX, offsetY);
        DrawTextLabel(dc, state->kind == CaptureLongRegion ? L"Drag scroll area, then choose scroll length" : L"Drag to capture region",
                      16 - offsetX, 58 - offsetY, RGB(245, 251, 248));
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
        if (wParam == VK_ESCAPE && state) {
            state->cancelled = true;
            state->done = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (state) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (state->showModeButtons) {
                bool hitMode = false;
                CaptureKind mode = OverlayModeAt(x, y, &hitMode);
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

static void UpdateLengthFromPoint(LengthState *state, int y) {
    int minLength = std::max(kLengthMin, RectHeight(state->selection));
    state->length = Clamp(y - state->selection.top, minLength, kLongCaptureMaxHeight);
}

static void DrawLengthOverlayOffset(LengthState *state, HWND hwnd, HDC dc, int offsetX, int offsetY) {
    HDC source = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(source, state->capture.bitmap);
    BitBlt(dc, -offsetX, -offsetY, state->capture.w, state->capture.h, source, 0, 0, SRCCOPY);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush shade(Color(150, 7, 11, 14));
        graphics.FillRectangle(&shade, 0, 0, state->capture.w, state->capture.h);
    }

    RECT selected = state->selection;
    BitBlt(dc, selected.left - offsetX, selected.top - offsetY, RectWidth(selected), RectHeight(selected),
           source, selected.left, selected.top, SRCCOPY);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        Pen accent(Color(255, 23, 198, 163), 2.4f);
        Pen yellow(Color(255, 255, 207, 90), 3.0f);
        int left = selected.left - offsetX;
        int top = selected.top - offsetY;
        int right = selected.right - offsetX;
        graphics.DrawRectangle(&accent, left, top, RectWidth(selected), RectHeight(selected));

        int markerX = right + 18;
        if (markerX > state->capture.w - offsetX - 28) {
            markerX = left - 18;
        }
        int markerStart = top;
        int visibleLength = std::min<int>(state->length, state->capture.h - selected.top - 28);
        int markerEnd = selected.top + visibleLength - offsetY;
        markerEnd = std::max(markerStart + 32, markerEnd);
        graphics.DrawLine(&yellow, markerX, markerStart, markerX, markerEnd);
        graphics.DrawLine(&yellow, markerX, markerEnd, markerX - 7, markerEnd - 10);
        graphics.DrawLine(&yellow, markerX, markerEnd, markerX + 7, markerEnd - 10);
    }

    wchar_t lengthText[96];
    wsprintfW(lengthText, L"Scroll length: %d px", state->length);
    RECT pill = {selected.left, selected.top - 34, selected.left + 190, selected.top - 6};
    if (pill.top < 8) {
        OffsetRect(&pill, 0, RectHeight(selected) + 42);
    }
    RECT drawPill = {pill.left - offsetX, pill.top - offsetY, pill.right - offsetX, pill.bottom - offsetY};
    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        GraphicsPath path;
        BuildRoundedRectPath(&path, static_cast<float>(drawPill.left), static_cast<float>(drawPill.top),
                             static_cast<float>(RectWidth(drawPill)), static_cast<float>(RectHeight(drawPill)), 8.0f);
        SolidBrush brush(Color(238, 16, 21, 24));
        graphics.FillPath(&brush, &path);
    } else {
        FillRectColor(dc, drawPill, RGB(16, 21, 24));
    }
    DrawTextLabel(dc, lengthText, drawPill.left + 10, drawPill.top + 5, RGB(245, 251, 248));
    DrawTextLabel(dc, L"Drag down to choose length. Wheel adjusts. Enter starts. Esc cancels.",
                  16 - offsetX, 18 - offsetY, RGB(245, 251, 248));

    SelectObject(source, old);
    DeleteDC(source);
    (void)hwnd;
}

static void PaintLengthBuffered(LengthState *state, HWND hwnd, HDC dc, const RECT &paint) {
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
        DrawLengthOverlayOffset(state, hwnd, dc, 0, 0);
        return;
    }

    HGDIOBJ old = SelectObject(memory, buffer);
    DrawLengthOverlayOffset(state, hwnd, memory, paint.left, paint.top);
    BitBlt(dc, paint.left, paint.top, w, h, memory, 0, 0, SRCCOPY);
    SelectObject(memory, old);
    DeleteObject(buffer);
    DeleteDC(memory);
}

static LRESULT CALLBACK LengthProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LengthState *state = reinterpret_cast<LengthState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        SetCursor(LoadCursor(NULL, IDC_SIZENS));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_SIZENS));
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
        if (wParam == VK_RETURN) {
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (state) {
            SetCapture(hwnd);
            state->dragging = true;
            UpdateLengthFromPoint(state, GET_Y_LPARAM(lParam));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state && state->dragging) {
            UpdateLengthFromPoint(state, GET_Y_LPARAM(lParam));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state && state->dragging) {
            ReleaseCapture();
            UpdateLengthFromPoint(state, GET_Y_LPARAM(lParam));
            state->done = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (state) {
            int step = std::max(80, RectHeight(state->selection) / 3);
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int minLength = std::max(kLengthMin, RectHeight(state->selection));
            state->length = Clamp(state->length + (delta < 0 ? step : -step), minLength, kLongCaptureMaxHeight);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            PaintLengthBuffered(state, hwnd, dc, ps.rcPaint);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static bool RunLengthOverlay(const ScreenCapture &capture, const RECT &selection, int *scrollLength) {
    LengthState state;
    state.capture = capture;
    state.selection = {selection.left - capture.x, selection.top - capture.y,
                       selection.right - capture.x, selection.bottom - capture.y};
    state.length = std::max(kLengthMin, RectHeight(state.selection));

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kLengthClass, L"Modern Screenshot Length",
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

    if (state.cancelled || state.length < kLengthMin) {
        return false;
    }
    *scrollLength = state.length;
    return true;
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
    ScreenCapture capture = CaptureVirtualScreen();
    if (!capture.bitmap) {
        g_captureActive = false;
        return;
    }

    RECT selection;
    CaptureKind kind = initialKind;
    if (RunSelectionOverlay(capture, &selection, &kind, true, initialKind)) {
        if (kind == CaptureLongRegion) {
            int desiredHeight = 0;
            if (RunLengthOverlay(capture, selection, &desiredHeight)) {
                DeleteObject(capture.bitmap);
                capture.bitmap = NULL;
                Sleep(120);

                int longH = 0;
                HBITMAP shot = CaptureLongSelection(selection, desiredHeight, &longH);
                if (shot) {
                    RunEditorModal(shot, RectWidth(selection), longH);
                    DeleteObject(shot);
                } else {
                    NotifyAction(g_hiddenWindow, L"Long capture failed",
                                 L"Could not stitch this area. Try a taller visible content area.");
                }
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

static void SendWheelDownAt(POINT point, HWND target) {
    HWND root = target ? GetAncestor(target, GA_ROOT) : NULL;
    if (root) {
        SetForegroundWindow(root);
    }
    SetCursorPos(point.x, point.y);
    Sleep(80);

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(-WHEEL_DELTA * kLongCaptureWheelNotches);
    SendInput(1, &input, sizeof(input));
    Sleep(kLongCaptureScrollDelayMs);
}

static HBITMAP CaptureLongSelection(const RECT &selection, int desiredHeight, int *outH) {
    int w = RectWidth(selection);
    int h = RectHeight(selection);
    if (w <= 16 || h <= 16) {
        return NULL;
    }
    desiredHeight = Clamp(desiredHeight, h, kLongCaptureMaxHeight);

    POINT center = {selection.left + w / 2, selection.top + h / 2};
    HWND target = WindowFromPoint(center);
    POINT oldCursor;
    GetCursorPos(&oldCursor);

    std::vector<LongFrame> frames;
    std::vector<BYTE> prevPixels;
    int totalH = 0;

    HBITMAP first = CaptureScreenRect(selection);
    if (!first || !ReadBitmapPixels(first, w, h, &prevPixels)) {
        if (first) {
            DeleteObject(first);
        }
        SetCursorPos(oldCursor.x, oldCursor.y);
        return NULL;
    }
    frames.push_back({first, 0, h});
    totalH = h;

    for (int i = 1; i < kLongCaptureMaxFrames && totalH < desiredHeight; ++i) {
        SendWheelDownAt(center, target);
        HBITMAP next = CaptureScreenRect(selection);
        std::vector<BYTE> nextPixels;
        if (!next || !ReadBitmapPixels(next, w, h, &nextPixels)) {
            if (next) {
                DeleteObject(next);
            }
            break;
        }

        if (AverageFrameDiff(prevPixels, nextPixels, w, h) <= 8) {
            DeleteObject(next);
            break;
        }

        int overlap = EstimateOverlap(prevPixels, nextPixels, w, h);
        int copyH = h - overlap;
        if (copyH < 32) {
            DeleteObject(next);
            break;
        }
        if (totalH + copyH > desiredHeight) {
            copyH = desiredHeight - totalH;
        }
        if (copyH <= 0) {
            DeleteObject(next);
            break;
        }

        frames.push_back({next, overlap, copyH});
        totalH += copyH;
        prevPixels.swap(nextPixels);
    }

    SetCursorPos(oldCursor.x, oldCursor.y);

    HBITMAP stitched = StitchLongFrames(frames, w, outH);
    for (LongFrame &frame : frames) {
        DeleteObject(frame.bitmap);
    }
    return stitched;
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

static void AddKeyOption(HWND combo, const KeyOption &option, bool selected) {
    LRESULT index = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(option.label));
    SendMessageW(combo, CB_SETITEMDATA, index, option.vk);
    if (selected) {
        SendMessageW(combo, CB_SETCURSEL, index, 0);
    }
}

static Settings ReadSettingsControls(HWND hwnd) {
    Settings next;
    next.modifiers = 0;
    if (SendDlgItemMessageW(hwnd, kSettingsCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        next.modifiers |= MOD_CONTROL;
    }
    if (SendDlgItemMessageW(hwnd, kSettingsAlt, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        next.modifiers |= MOD_ALT;
    }
    if (SendDlgItemMessageW(hwnd, kSettingsShift, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        next.modifiers |= MOD_SHIFT;
    }
    if (SendDlgItemMessageW(hwnd, kSettingsWin, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        next.modifiers |= MOD_WIN;
    }
    if (next.modifiers == 0) {
        next.modifiers = MOD_CONTROL;
    }

    HWND combo = GetDlgItem(hwnd, kSettingsKey);
    LRESULT selected = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (selected >= 0) {
        next.vk = static_cast<UINT>(SendMessageW(combo, CB_GETITEMDATA, selected, 0));
    } else {
        next.vk = g_settings.vk;
    }
    return next;
}

static void UpdateSettingsStatus(HWND hwnd, const std::wstring &message) {
    SetWindowTextW(GetDlgItem(hwnd, kSettingsStatus), message.c_str());
}

static LRESULT CALLBACK SettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"Hotkey", WS_CHILD | WS_VISIBLE, 22, 22, 220, 20,
                      hwnd, NULL, g_instance, NULL);
        CreateWindowW(L"BUTTON", L"Ctrl", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 22, 56, 70, 24,
                      hwnd, reinterpret_cast<HMENU>(kSettingsCtrl), g_instance, NULL);
        CreateWindowW(L"BUTTON", L"Alt", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 94, 56, 64, 24,
                      hwnd, reinterpret_cast<HMENU>(kSettingsAlt), g_instance, NULL);
        CreateWindowW(L"BUTTON", L"Shift", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 160, 56, 78, 24,
                      hwnd, reinterpret_cast<HMENU>(kSettingsShift), g_instance, NULL);
        CreateWindowW(L"BUTTON", L"Win", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 240, 56, 70, 24,
                      hwnd, reinterpret_cast<HMENU>(kSettingsWin), g_instance, NULL);
        HWND combo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   22, 92, 288, 240, hwnd, reinterpret_cast<HMENU>(kSettingsKey), g_instance, NULL);
        for (int i = 0; i < KeyOptionCount(); ++i) {
            AddKeyOption(combo, kKeyOptions[i], kKeyOptions[i].vk == g_settings.vk);
        }

        SendDlgItemMessageW(hwnd, kSettingsCtrl, BM_SETCHECK, (g_settings.modifiers & MOD_CONTROL) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessageW(hwnd, kSettingsAlt, BM_SETCHECK, (g_settings.modifiers & MOD_ALT) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessageW(hwnd, kSettingsShift, BM_SETCHECK, (g_settings.modifiers & MOD_SHIFT) ? BST_CHECKED : BST_UNCHECKED, 0);
        SendDlgItemMessageW(hwnd, kSettingsWin, BM_SETCHECK, (g_settings.modifiers & MOD_WIN) ? BST_CHECKED : BST_UNCHECKED, 0);
        CreateWindowW(L"BUTTON", L"Start with Windows", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 22, 132, 220, 24,
                      hwnd, reinterpret_cast<HMENU>(kSettingsStartup), g_instance, NULL);
        SendDlgItemMessageW(hwnd, kSettingsStartup, BM_SETCHECK, IsStartupEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 22, 166, 320, 24,
                      hwnd, reinterpret_cast<HMENU>(kSettingsStatus), g_instance, NULL);
        CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 154, 206, 82, 32,
                      hwnd, reinterpret_cast<HMENU>(kSettingsApply), g_instance, NULL);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 244, 206, 82, 32,
                      hwnd, reinterpret_cast<HMENU>(kSettingsCancel), g_instance, NULL);
        UpdateSettingsStatus(hwnd, L"Current: " + HotkeyText(g_settings));
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kSettingsApply) {
            Settings old = g_settings;
            g_settings = ReadSettingsControls(hwnd);
            if (RegisterConfiguredHotkey(g_hiddenWindow, false)) {
                SaveSettings();
                bool startup = SendDlgItemMessageW(hwnd, kSettingsStartup, BM_GETCHECK, 0, 0) == BST_CHECKED;
                if (SetStartupEnabled(startup)) {
                    ShowTrayBalloon(g_hiddenWindow, L"Settings updated", HotkeyText(g_settings));
                    DestroyWindow(hwnd);
                } else {
                    UpdateSettingsStatus(hwnd, L"Could not update startup setting");
                    MessageBoxW(hwnd, L"Could not update the Windows startup setting.",
                                L"ModernScreenshot", MB_ICONWARNING);
                }
            } else {
                g_settings = old;
                RegisterConfiguredHotkey(g_hiddenWindow, false);
                UpdateSettingsStatus(hwnd, L"Conflict: " + HotkeyText(ReadSettingsControls(hwnd)));
                MessageBoxW(hwnd, L"This hotkey is already in use. Choose another combination.",
                            L"ModernScreenshot", MB_ICONWARNING);
            }
            return 0;
        }
        if (LOWORD(wParam) == kSettingsCancel) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_ERASEBKGND: {
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRectColor(reinterpret_cast<HDC>(wParam), rect, RGB(245, 248, 247));
        return 1;
    }
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

    g_settingsWindow = CreateWindowExW(WS_EX_TOOLWINDOW, kSettingsClass, L"ModernScreenshot Settings",
                                       WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                                       370, 300, NULL, NULL, g_instance, NULL);
    if (g_settingsWindow) {
        ShowWindow(g_settingsWindow, SW_SHOW);
        SetForegroundWindow(g_settingsWindow);
    }
}

static void ShowTrayMenu(HWND hwnd) {
    POINT cursor;
    GetCursorPos(&cursor);
    HMENU menu = CreatePopupMenu();
    std::wstring capture = L"Capture (" + HotkeyText(g_settings) + L")";
    AppendMenuW(menu, MF_STRING, kMenuCapture, capture.c_str());
    AppendMenuW(menu, MF_STRING, kMenuLongCapture, L"Long Capture...");
    AppendMenuW(menu, MF_STRING, kMenuSettings, L"Settings...");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"Exit");
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursor.x, cursor.y, 0, hwnd, NULL);
    DestroyMenu(menu);
}

static LRESULT CALLBACK HiddenProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case kTrayMessage:
        if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
            ShowTrayMenu(hwnd);
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

    WNDCLASSW length = {};
    length.lpfnWndProc = LengthProc;
    length.hInstance = g_instance;
    length.hCursor = LoadCursor(NULL, IDC_SIZENS);
    length.hbrBackground = NULL;
    length.lpszClassName = kLengthClass;
    RegisterClassW(&length);

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
}

static int RunDaemon() {
    HWND hwnd = CreateWindowExW(0, kHiddenClass, L"ModernScreenshot", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, g_instance, NULL);
    if (!hwnd) {
        return 1;
    }
    g_hiddenWindow = hwnd;
    AddTrayIcon(hwnd);

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
    StartGdiplus();
    LoadSettings();
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
    } else if (argc > 1 && wcscmp(argv[1], L"--long") == 0) {
        RunLongCapture();
        status = 0;
    } else if (argc == 1) {
        status = RunDaemon();
    } else {
        MessageBoxW(NULL,
                    L"Usage:\nModernScreenshot.exe\nModernScreenshot.exe --once\nModernScreenshot.exe --long\nModernScreenshot.exe --self-test PATH",
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
