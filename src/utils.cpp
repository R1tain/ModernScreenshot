#include "utils.h"

namespace ModernScreenshot {

std::wstring ConfigPath() {
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

std::wstring VkName(UINT vk) {
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

std::wstring HotkeyText(const Settings &settings) {
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

void LoadSettings(Settings &settings) {
    std::wstring path = ConfigPath();
    settings.modifiers = GetPrivateProfileIntW(L"hotkey", L"modifiers", MOD_CONTROL, path.c_str());
    settings.vk = GetPrivateProfileIntW(L"hotkey", L"vk", '1', path.c_str());
    if ((settings.modifiers & (MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN)) == 0) {
        settings.modifiers = MOD_CONTROL;
    }
}

void SaveSettings(const Settings &settings) {
    std::wstring path = ConfigPath();
    wchar_t value[32];
    wsprintfW(value, L"%u", settings.modifiers);
    WritePrivateProfileStringW(L"hotkey", L"modifiers", value, path.c_str());
    wsprintfW(value, L"%u", settings.vk);
    WritePrivateProfileStringW(L"hotkey", L"vk", value, path.c_str());
}

std::wstring ModulePath() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return path;
}

std::wstring QuotedPath(const std::wstring &path) {
    return L"\"" + path + L"\"";
}

bool IsStartupEnabled() {
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

bool SetStartupEnabled(bool enabled) {
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

void EnsureStartupPathSynced() {
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

std::wstring BuildScreenshotPath() {
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

bool SetClipboardPngAndDIB(HBITMAP bitmap) {
    if (!bitmap) return false;
    if (!OpenClipboard(NULL)) return false;
    EmptyClipboard();
    if (!SetClipboardData(CF_BITMAP, bitmap)) {
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

void BuildRoundedRectPath(GraphicsPath *path, float x, float y, float w, float h, float radius) {
    float d = radius * 2.0f;
    path->Reset();
    path->AddArc(x, y, d, d, 180.0f, 90.0f);
    path->AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path->AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path->AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path->CloseFigure();
}

} // namespace ModernScreenshot
