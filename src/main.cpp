#include "common.h"
#include "app.h"
#include "graphics.h"
#include "utils.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDPIAware();

    ModernScreenshot::Settings settings;
    ModernScreenshot::LoadSettings(settings);
    ModernScreenshot::SetAppSettings(settings);

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int status = 0;

    if (argc > 1 && wcscmp(argv[1], L"--version") == 0) {
        ModernScreenshot::PrintVersionToConsole();
        if (argv) LocalFree(argv);
        return 0;
    }

    HANDLE singleInstanceMutex = NULL;
    if (argc == 1) {
        singleInstanceMutex = CreateMutexW(NULL, FALSE, ModernScreenshot::kSingleInstanceName);
        if (singleInstanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
            ModernScreenshot::WakePreviousInstance();
            CloseHandle(singleInstanceMutex);
            if (argv) LocalFree(argv);
            return 0;
        }
    }

    ModernScreenshot::RegisterWindowClasses(instance);

    if (argc > 2 && wcscmp(argv[1], L"--self-test") == 0) {
        status = ModernScreenshot::SelfTestExport(argv[2]) ? 0 : 1;
    } else if (argc > 1 && wcscmp(argv[1], L"--once") == 0) {
        ModernScreenshot::RunCapture();
        status = 0;
    } else if (argc > 1 && wcscmp(argv[1], L"--long") == 0) {
        ModernScreenshot::RunLongCapture();
        status = 0;
    } else if (argc == 1) {
        status = ModernScreenshot::RunDaemon(instance);
    } else {
        MessageBoxW(NULL,
                    L"Usage:\nModernScreenshot.exe\nModernScreenshot.exe --once\nModernScreenshot.exe --long\nModernScreenshot.exe --self-test PATH\nModernScreenshot.exe --version",
                    L"ModernScreenshot", MB_OK);
        status = 2;
    }

    if (argv) LocalFree(argv);

    ModernScreenshot::UnregisterWindowClasses(instance);
    ModernScreenshot::StopGdiplus();

    if (singleInstanceMutex) {
        CloseHandle(singleInstanceMutex);
    }
    return status;
}
