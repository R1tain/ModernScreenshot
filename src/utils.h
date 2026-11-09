#pragma once

#include "common.h"

namespace ModernScreenshot {

std::wstring ConfigPath();
std::wstring VkName(UINT vk);
std::wstring HotkeyText(const Settings &settings);
void LoadSettings(Settings &settings);
void SaveSettings(const Settings &settings);

std::wstring ModulePath();
std::wstring QuotedPath(const std::wstring &path);
bool IsStartupEnabled();
bool SetStartupEnabled(bool enabled);
void EnsureStartupPathSynced();

std::wstring BuildScreenshotPath();
bool SetClipboardPngAndDIB(HBITMAP bitmap);
void BuildRoundedRectPath(GraphicsPath *path, float x, float y, float w, float h, float radius);

} // namespace ModernScreenshot
