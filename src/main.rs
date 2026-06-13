use windows::{
    core::*,
    Win32::Foundation::*,
    Win32::Graphics::Gdi::*,
    Win32::UI::WindowsAndMessaging::*,
    Win32::UI::Input::KeyboardAndMouse::*,
    Win32::System::LibraryLoader::GetModuleHandleW,
    Win32::UI::Shell::*,
};

const CLASS_NAME: PCWSTR = w!("ModernScreenshotClass");
const APP_NAME: PCWSTR = w!("ModernScreenshot");
const HOTKEY_ID: i32 = 1;

static mut APP_INSTANCE: Option<App> = None;

struct App {
    hwnd: HWND,
    tray_icon: NOTIFYICONDATAW,
}

impl App {
    fn new() -> Result<Self> {
        let h_instance = unsafe { GetModuleHandleW(None)? };

        // 注册窗口类
        let wc = WNDCLASSW {
            lpfnWndProc: Some(wndproc),
            hInstance: h_instance.into(),
            lpszClassName: CLASS_NAME,
            hCursor: unsafe { LoadCursorW(None, IDC_ARROW)? },
            ..Default::default()
        };

        unsafe { RegisterClassW(&wc) };

        // 创建隐藏窗口
        let hwnd = unsafe {
            CreateWindowExW(
                WINDOW_EX_STYLE(0),
                CLASS_NAME,
                APP_NAME,
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                None,
                None,
                h_instance,
                None,
            )?
        };

        // 注册全局热键 Ctrl+1
        unsafe {
            RegisterHotKey(
                hwnd,
                HOTKEY_ID,
                HOT_KEY_MODIFIERS(MOD_CONTROL.0),
                0x31, // VK_1
            )?;
        }

        // 创建托盘图标
        let mut tray_icon = NOTIFYICONDATAW {
            cbSize: std::mem::size_of::<NOTIFYICONDATAW>() as u32,
            hWnd: hwnd,
            uID: 1,
            uFlags: NIF_ICON | NIF_MESSAGE | NIF_TIP,
            uCallbackMessage: WM_USER + 1,
            hIcon: unsafe { LoadIconW(None, IDI_APPLICATION)? },
            ..Default::default()
        };

        let tip = "ModernScreenshot (Ctrl+1)";
        let tip_wide: Vec<u16> = tip.encode_utf16().chain(std::iter::once(0)).collect();
        tray_icon.szTip[..tip_wide.len()].copy_from_slice(&tip_wide);

        unsafe { Shell_NotifyIconW(NIM_ADD, &tray_icon)? };

        Ok(Self { hwnd, tray_icon })
    }

    fn start_capture(&self) {
        // TODO: 实现截图逻辑
        unsafe {
            MessageBoxW(
                self.hwnd,
                w!("截图功能开发中..."),
                w!("ModernScreenshot"),
                MB_OK | MB_ICONINFORMATION,
            );
        }
    }
}

impl Drop for App {
    fn drop(&mut self) {
        unsafe {
            Shell_NotifyIconW(NIM_DELETE, &self.tray_icon).ok();
            UnregisterHotKey(self.hwnd, HOTKEY_ID).ok();
        }
    }
}

unsafe extern "system" fn wndproc(
    hwnd: HWND,
    msg: u32,
    wparam: WPARAM,
    lparam: LPARAM,
) -> LRESULT {
    match msg {
        WM_HOTKEY => {
            if wparam.0 as i32 == HOTKEY_ID {
                if let Some(app) = &APP_INSTANCE {
                    app.start_capture();
                }
            }
            LRESULT(0)
        }
        WM_DESTROY => {
            PostQuitMessage(0);
            LRESULT(0)
        }
        _ => DefWindowProcW(hwnd, msg, wparam, lparam),
    }
}

fn main() -> Result<()> {
    unsafe {
        APP_INSTANCE = Some(App::new()?);

        let mut msg = MSG::default();
        while GetMessageW(&mut msg, None, 0, 0).into() {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        APP_INSTANCE = None;
    }

    Ok(())
}
