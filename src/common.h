#pragma once

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

namespace ModernScreenshot {

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

static const wchar_t *const kVersion = WIDEN(VERSION);
static const wchar_t *const kHiddenClass = L"ModernScreenshotHidden";
static const wchar_t *const kOverlayClass = L"ModernScreenshotOverlay";
static const wchar_t *const kLongSessionClass = L"ModernScreenshotLongSession";
static const wchar_t *const kEditorClass = L"ModernScreenshotEditor";
static const wchar_t *const kSettingsClass = L"ModernScreenshotSettings";
static const wchar_t *const kTrayPopupClass = L"ModernScreenshotTrayPopup";

static constexpr int kHotkeyId = 1001;
static constexpr UINT kTrayMessage = WM_APP + 1;
static constexpr UINT kMenuCapture = 2001;
static constexpr UINT kMenuLongCapture = 2002;
static constexpr UINT kMenuSettings = 2003;
static constexpr UINT kMenuExit = 2004;

static constexpr int kToolbarH = 64;
static constexpr int kButtonW = 58;
static constexpr int kButtonH = 46;
static constexpr int kButtonGap = 8;
static constexpr int kEditorMargin = 80;

static constexpr int kLongCaptureMaxFrames = 512;
static constexpr int kLongCaptureMaxHeight = 80000;
static constexpr int kLongScrollWaitMs = 240;
static constexpr int kLongScrollSettleMs = 80;
static constexpr int kLongScrollSettleAttempts = 5;
static constexpr int kLongOverlapErrorLimit = 30;
static constexpr int kLongSessionW = 360;
static constexpr int kLongSessionH = 520;

static constexpr int kSettingsApply = 3001;
static constexpr int kSettingsCancel = 3002;
static constexpr int kSettingsHotkeyCapture = 3003;
static constexpr int kSettingsStartup = 3009;

static constexpr int kOverlayModeW = 116;
static constexpr int kOverlayModeH = 34;
static constexpr int kOverlayModeGap = 8;
static constexpr int kLongSessionDone = 4101;
static constexpr int kLongSessionCancel = 4102;

static const wchar_t *const kSingleInstanceName = L"Local\\ModernScreenshot.SingleInstance";
static const wchar_t *const kWakeupMessageName = L"ModernScreenshotWakeup";

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

enum SettingsHit {
    SettingsHitNone,
    SettingsHitHotkey,
    SettingsHitStartup,
    SettingsHitApply,
    SettingsHitCancel,
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

struct Settings {
    UINT modifiers = MOD_CONTROL;
    UINT vk = '1';
};

struct KeyOption {
    const wchar_t *label;
    UINT vk;
};

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

inline int ButtonCount() {
    return static_cast<int>(sizeof(kButtons) / sizeof(kButtons[0]));
}

inline int KeyOptionCount() {
    return static_cast<int>(sizeof(kKeyOptions) / sizeof(kKeyOptions[0]));
}

inline int ToolbarWidth() {
    return 24 + ButtonCount() * kButtonW + (ButtonCount() - 1) * kButtonGap;
}

inline int Clamp(int value, int minValue, int maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

inline int RectWidth(const RECT &rect) {
    return rect.right - rect.left;
}

inline int RectHeight(const RECT &rect) {
    return rect.bottom - rect.top;
}

inline RECT NormalizeRectPoints(POINT a, POINT b) {
    RECT rect;
    rect.left = a.x < b.x ? a.x : b.x;
    rect.top = a.y < b.y ? a.y : b.y;
    rect.right = a.x > b.x ? a.x : b.x;
    rect.bottom = a.y > b.y ? a.y : b.y;
    return rect;
}

inline RECT ClampRect(RECT rect, int left, int top, int right, int bottom) {
    rect.left = Clamp(rect.left, left, right);
    rect.right = Clamp(rect.right, left, right);
    rect.top = Clamp(rect.top, top, bottom);
    rect.bottom = Clamp(rect.bottom, top, bottom);
    if (rect.right < rect.left) rect.right = rect.left;
    if (rect.bottom < rect.top) rect.bottom = rect.top;
    return rect;
}

inline RECT ButtonRect(int index) {
    RECT rect;
    rect.left = 12 + index * (kButtonW + kButtonGap);
    rect.top = 8;
    rect.right = rect.left + kButtonW;
    rect.bottom = rect.top + kButtonH;
    return rect;
}

inline RECT OverlayModeRect(int captureW, int captureH, int index) {
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

inline RECT OverlayHintRect(int captureW, int captureH) {
    int w = 360;
    int h = 36;
    int x = (captureW - w) / 2;
    if (x < 16) x = 16;
    int y = captureH - 96 - h - 14;
    if (y < 16) y = 16;
    RECT rect = {x, y, x + w, y + h};
    return rect;
}

inline bool PointInRect(int x, int y, const RECT &rect) {
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

inline bool RectIntersects(const RECT &a, const RECT &b) {
    RECT intersection;
    return IntersectRect(&intersection, &a, &b) != FALSE;
}

inline bool IsRectEmptyOrInvalid(const RECT &rect) {
    return rect.right <= rect.left || rect.bottom <= rect.top;
}

inline RECT ExpandedRect(RECT rect, int amount, int maxW, int maxH) {
    rect.left = Clamp(rect.left - amount, 0, maxW);
    rect.top = Clamp(rect.top - amount, 0, maxH);
    rect.right = Clamp(rect.right + amount, 0, maxW);
    rect.bottom = Clamp(rect.bottom + amount, 0, maxH);
    return rect;
}

inline Action ButtonAt(int x, int y, Tool *tool) {
    for (int i = 0; i < ButtonCount(); ++i) {
        if (PointInRect(x, y, ButtonRect(i))) {
            *tool = kButtons[i].tool;
            return kButtons[i].action;
        }
    }
    return ActionNone;
}

inline CaptureKind OverlayModeAt(int captureW, int captureH, int x, int y, bool *hit) {
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

inline HWND RootWindow(HWND hwnd) {
    return hwnd ? GetAncestor(hwnd, GA_ROOT) : NULL;
}

} // namespace ModernScreenshot
