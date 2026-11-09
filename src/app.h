#pragma once

#include "common.h"
#include "graphics.h"
#include "utils.h"

namespace ModernScreenshot {

HINSTANCE GetAppInstance();
HWND GetAppHiddenWindow();
HICON GetAppTrayIcon();
Settings GetAppSettings();
void SetAppSettings(const Settings &s);

bool RegisterConfiguredHotkey(HWND hwnd, bool notify);
void ShowTrayBalloon(HWND hwnd, const std::wstring &title, const std::wstring &message);
void NotifyAction(HWND owner, const std::wstring &title, const std::wstring &message);

void RunCapture();
void RunLongCapture();
void RunSelectedCapture(CaptureKind initialKind);

bool SelfTestExport(const std::wstring &path);
void RegisterWindowClasses(HINSTANCE instance);
void UnregisterWindowClasses(HINSTANCE instance);
bool PrintVersionToConsole();
bool WakePreviousInstance();

int RunDaemon(HINSTANCE instance);

} // namespace ModernScreenshot
