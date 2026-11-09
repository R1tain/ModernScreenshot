#pragma once

#include "common.h"
#include "graphics.h"

namespace ModernScreenshot {

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

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void ShowSettingsWindow(HINSTANCE instance, HICON trayIcon, HWND hiddenWnd);

} // namespace ModernScreenshot
