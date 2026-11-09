#pragma once

#include "common.h"
#include "graphics.h"

namespace ModernScreenshot {

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

LRESULT CALLBACK TrayPopupProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void ShowTrayPopup(HWND hwnd, HINSTANCE instance, HWND hiddenWnd, const Settings &settings);

} // namespace ModernScreenshot
