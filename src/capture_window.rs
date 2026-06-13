use windows::{
    core::*,
    Win32::Foundation::*,
    Win32::Graphics::Gdi::*,
    Win32::UI::WindowsAndMessaging::*,
    Win32::System::LibraryLoader::GetModuleHandleW,
};
use crate::capture::Screenshot;
use crate::toolbar::Toolbar;
use crate::drawing::{ToolMode, DrawAction};
use crate::save::{save_bitmap_to_file, copy_bitmap_to_clipboard};
use crate::keys::VK_ESCAPE;

const CAPTURE_CLASS: PCWSTR = w!("CaptureWindowClass");

pub struct CaptureWindow {
    hwnd: HWND,
    screenshot: Option<Screenshot>,
    selecting: bool,
    selected: bool,
    start_x: i32,
    start_y: i32,
    end_x: i32,
    end_y: i32,
    sel_x: i32,
    sel_y: i32,
    sel_w: i32,
    sel_h: i32,
    toolbar: Option<Toolbar>,
    current_tool: ToolMode,
    drawing: bool,
    draw_start_x: i32,
    draw_start_y: i32,
    actions: Vec<DrawAction>,
}

static mut CAPTURE_WINDOW: Option<*mut CaptureWindow> = None;

impl CaptureWindow {
    pub unsafe fn new() -> Result<Self> {
        let h_instance = GetModuleHandleW(None)?;

        // 注册窗口类
        let wc = WNDCLASSW {
            lpfnWndProc: Some(capture_wndproc),
            hInstance: h_instance.into(),
            lpszClassName: CAPTURE_CLASS,
            hCursor: LoadCursorW(None, IDC_CROSS)?,
            hbrBackground: CreateSolidBrush(COLORREF(0x00000000)).into(),
            ..Default::default()
        };

        RegisterClassW(&wc);

        Ok(Self {
            hwnd: HWND(std::ptr::null_mut()),
            screenshot: None,
            selecting: false,
            selected: false,
            start_x: 0,
            start_y: 0,
            end_x: 0,
            end_y: 0,
            sel_x: 0,
            sel_y: 0,
            sel_w: 0,
            sel_h: 0,
            toolbar: None,
            current_tool: ToolMode::None,
            drawing: false,
            draw_start_x: 0,
            draw_start_y: 0,
            actions: Vec::new(),
        })
    }

    pub unsafe fn show(&mut self) -> Result<()> {
        // 捕获截图
        self.screenshot = Some(Screenshot::capture().expect("Screenshot capture failed"));

        let screen_width = GetSystemMetrics(SM_CXSCREEN);
        let screen_height = GetSystemMetrics(SM_CYSCREEN);

        let h_instance = GetModuleHandleW(None)?;

        // 创建全屏窗口
        self.hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED,
            CAPTURE_CLASS,
            w!("Screenshot"),
            WS_POPUP,
            0, 0,
            screen_width, screen_height,
            None,
            None,
            h_instance,
            None,
        )?;

        // 设置半透明
        SetLayeredWindowAttributes(self.hwnd, COLORREF(0), 200, LWA_ALPHA)?;

        // 保存实例指针
        CAPTURE_WINDOW = Some(self as *mut CaptureWindow);

        ShowWindow(self.hwnd, SW_SHOW);
        UpdateWindow(self.hwnd);

        Ok(())
    }

    unsafe fn on_paint(&self) {
        let mut ps = PAINTSTRUCT::default();
        let hdc = BeginPaint(self.hwnd, &mut ps);

        if let Some(ref screenshot) = self.screenshot {
            // 绘制截图
            screenshot.draw_to(hdc, 0, 0, screenshot.width, screenshot.height).ok();

            // 绘制半透明遮罩
            let brush = CreateSolidBrush(COLORREF(0x000000));
            let mut rect = RECT {
                left: 0,
                top: 0,
                right: screenshot.width,
                bottom: screenshot.height,
            };

            if self.selecting {
                let x = self.start_x.min(self.end_x);
                let y = self.start_y.min(self.end_y);
                let w = (self.start_x - self.end_x).abs();
                let h = (self.start_y - self.end_y).abs();

                // 绘制选区边框
                let pen = CreatePen(PS_SOLID, 2, COLORREF(0x0078D7));
                let old_pen = SelectObject(hdc, pen);
                let old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

                Rectangle(hdc, x, y, x + w, y + h);

                SelectObject(hdc, old_pen);
                SelectObject(hdc, old_brush);
                DeleteObject(pen);
            } else if self.selected {
                // 绘制已确定的选区
                let pen = CreatePen(PS_SOLID, 2, COLORREF(0x0078D7));
                let old_pen = SelectObject(hdc, pen);
                let old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

                Rectangle(hdc, self.sel_x, self.sel_y, self.sel_x + self.sel_w, self.sel_y + self.sel_h);

                SelectObject(hdc, old_pen);
                SelectObject(hdc, old_brush);
                DeleteObject(pen);

                // 绘制所有标注
                for action in &self.actions {
                    action.draw(hdc);
                }
            }

            DeleteObject(brush);
        }

        EndPaint(self.hwnd, &ps);
    }

    unsafe fn on_lbutton_down(&mut self, x: i32, y: i32) {
        if !self.selected {
            // 开始选区
            self.selecting = true;
            self.start_x = x;
            self.start_y = y;
            self.end_x = x;
            self.end_y = y;
        } else if self.current_tool != ToolMode::None {
            // 开始绘制
            self.drawing = true;
            self.draw_start_x = x;
            self.draw_start_y = y;
        }
    }

    unsafe fn on_mouse_move(&mut self, x: i32, y: i32) {
        if self.selecting {
            self.end_x = x;
            self.end_y = y;
            InvalidateRect(self.hwnd, None, FALSE);
        } else if self.drawing {
            // 实时预览（简化版：直接添加到 actions）
            InvalidateRect(self.hwnd, None, FALSE);
        }
    }

    unsafe fn on_lbutton_up(&mut self, x: i32, y: i32) {
        if self.selecting {
            self.selecting = false;

            // 计算选区
            self.sel_x = self.start_x.min(self.end_x);
            self.sel_y = self.start_y.min(self.end_y);
            self.sel_w = (self.start_x - self.end_x).abs();
            self.sel_h = (self.start_y - self.end_y).abs();

            if self.sel_w > 10 && self.sel_h > 10 {
                self.selected = true;

                // 显示工具栏
                let toolbar_x = self.sel_x;
                let toolbar_y = self.sel_y + self.sel_h + 10;

                let mut toolbar = Toolbar::new(self.hwnd, toolbar_x, toolbar_y).expect("Failed to create toolbar");
                toolbar.show();
                self.toolbar = Some(toolbar);

                InvalidateRect(self.hwnd, None, FALSE);
            }
        } else if self.drawing {
            self.drawing = false;

            // 添加绘图动作
            let action = DrawAction {
                tool: self.current_tool,
                start_x: self.draw_start_x,
                start_y: self.draw_start_y,
                end_x: x,
                end_y: y,
                color: COLORREF(0xFF0000), // 红色
                width: 3,
                text: None,
            };

            self.actions.push(action);
            InvalidateRect(self.hwnd, None, FALSE);
        }
    }

    pub unsafe fn save_screenshot(&mut self) {
        if let Some(ref screenshot) = self.screenshot {
            save_bitmap_to_file(screenshot.hdc, self.sel_x, self.sel_y, self.sel_w, self.sel_h).ok();
        }
    }

    pub unsafe fn copy_screenshot(&mut self) {
        if let Some(ref screenshot) = self.screenshot {
            copy_bitmap_to_clipboard(self.hwnd, screenshot.hdc, self.sel_x, self.sel_y, self.sel_w, self.sel_h).ok();
        }
    }

    unsafe fn close(&mut self) {
        if !self.hwnd.is_invalid() {
            DestroyWindow(self.hwnd);
            self.hwnd = HWND(std::ptr::null_mut());
        }
        self.screenshot = None;
        self.toolbar = None;
        CAPTURE_WINDOW = None;
    }
}

unsafe extern "system" fn capture_wndproc(
    hwnd: HWND,
    msg: u32,
    wparam: WPARAM,
    lparam: LPARAM,
) -> LRESULT {
    const WM_TOOLBAR_ACTION: u32 = 0x0400 + 100; // WM_USER + 100

    if let Some(window_ptr) = CAPTURE_WINDOW {
        let window = &mut *window_ptr;

        match msg {
            WM_PAINT => {
                window.on_paint();
                return LRESULT(0);
            }
            WM_LBUTTONDOWN => {
                let x = (lparam.0 & 0xFFFF) as i16 as i32;
                let y = ((lparam.0 >> 16) & 0xFFFF) as i16 as i32;
                window.on_lbutton_down(x, y);
                return LRESULT(0);
            }
            WM_MOUSEMOVE => {
                let x = (lparam.0 & 0xFFFF) as i16 as i32;
                let y = ((lparam.0 >> 16) & 0xFFFF) as i16 as i32;
                window.on_mouse_move(x, y);
                return LRESULT(0);
            }
            WM_LBUTTONUP => {
                let x = (lparam.0 & 0xFFFF) as i16 as i32;
                let y = ((lparam.0 >> 16) & 0xFFFF) as i16 as i32;
                window.on_lbutton_up(x, y);
                return LRESULT(0);
            }
            WM_TOOLBAR_ACTION => {
                // 处理工具栏按钮点击
                match wparam.0 {
                    1 => window.save_screenshot(), // 保存
                    2 => window.copy_screenshot(), // 复制
                    3 => window.close(),           // 取消
                    _ => {}
                }
                return LRESULT(0);
            }
            WM_KEYDOWN => {
                if wparam.0 == VK_ESCAPE.0 as usize {
                    window.close();
                    return LRESULT(0);
                }
            }
            WM_DESTROY => {
                window.close();
                return LRESULT(0);
            }
            _ => {}
        }
    }

    DefWindowProcW(hwnd, msg, wparam, lparam)
}
