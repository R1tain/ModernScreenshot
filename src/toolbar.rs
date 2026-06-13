use windows::{
    core::*,
    Win32::Foundation::*,
    Win32::Graphics::Gdi::*,
    Win32::UI::WindowsAndMessaging::*,
    Win32::System::LibraryLoader::GetModuleHandleW,
};
use crate::drawing::ToolMode;

const TOOLBAR_CLASS: PCWSTR = w!("ToolbarWindowClass");
const TOOLBAR_HEIGHT: i32 = 50;
const TOOLBAR_WIDTH: i32 = 500;
const BUTTON_SIZE: i32 = 44;
const BUTTON_SPACING: i32 = 4;

#[derive(Clone, Copy, PartialEq)]
pub enum ButtonAction {
    Tool(ToolMode),
    Save,
    Copy,
    Cancel,
}

pub struct Toolbar {
    pub hwnd: HWND,
    pub selected_tool: ToolMode,
    buttons: Vec<ToolButton>,
    pub parent: HWND,
}

struct ToolButton {
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    action: ButtonAction,
    icon: String,
    hovered: bool,
    is_primary: bool, // 保存按钮高亮
}

static mut TOOLBAR_INSTANCE: Option<*mut Toolbar> = None;

// 自定义消息
const WM_TOOLBAR_ACTION: u32 = WM_USER + 100;

impl Toolbar {
    pub unsafe fn new(parent: HWND, x: i32, y: i32) -> Result<Self> {
        let h_instance = GetModuleHandleW(None)?;

        // 注册窗口类（只注册一次）
        let wc = WNDCLASSW {
            lpfnWndProc: Some(toolbar_wndproc),
            hInstance: h_instance.into(),
            lpszClassName: TOOLBAR_CLASS,
            hbrBackground: CreateSolidBrush(COLORREF(0x2E2A2E)).into(),
            ..Default::default()
        };

        RegisterClassW(&wc);

        let hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED,
            TOOLBAR_CLASS,
            w!("Toolbar"),
            WS_POPUP,
            x, y,
            TOOLBAR_WIDTH, TOOLBAR_HEIGHT,
            parent,
            None,
            h_instance,
            None,
        )?;

        SetLayeredWindowAttributes(hwnd, COLORREF(0), 240, LWA_ALPHA)?;

        let mut toolbar = Self {
            hwnd,
            parent,
            selected_tool: ToolMode::None,
            buttons: Vec::new(),
        };

        toolbar.create_buttons();

        Ok(toolbar)
    }

    fn create_buttons(&mut self) {
        let mut x = 10;
        let y = 7;

        // 工具按钮
        self.buttons.push(ToolButton {
            x, y, width: BUTTON_SIZE, height: 36,
            action: ButtonAction::Tool(ToolMode::Rectangle),
            icon: "□".to_string(),
            hovered: false,
            is_primary: false,
        });
        x += BUTTON_SIZE + BUTTON_SPACING;

        self.buttons.push(ToolButton {
            x, y, width: BUTTON_SIZE, height: 36,
            action: ButtonAction::Tool(ToolMode::Arrow),
            icon: "→".to_string(),
            hovered: false,
            is_primary: false,
        });
        x += BUTTON_SIZE + BUTTON_SPACING;

        self.buttons.push(ToolButton {
            x, y, width: BUTTON_SIZE, height: 36,
            action: ButtonAction::Tool(ToolMode::Pen),
            icon: "✎".to_string(),
            hovered: false,
            is_primary: false,
        });
        x += BUTTON_SIZE + BUTTON_SPACING;

        self.buttons.push(ToolButton {
            x, y, width: BUTTON_SIZE, height: 36,
            action: ButtonAction::Tool(ToolMode::Mosaic),
            icon: "⊞".to_string(),
            hovered: false,
            is_primary: false,
        });
        x += BUTTON_SIZE + 20;

        // 操作按钮
        self.buttons.push(ToolButton {
            x, y, width: BUTTON_SIZE, height: 36,
            action: ButtonAction::Save,
            icon: "💾".to_string(),
            hovered: false,
            is_primary: true, // 保存按钮主色
        });
        x += BUTTON_SIZE + BUTTON_SPACING;

        self.buttons.push(ToolButton {
            x, y, width: BUTTON_SIZE, height: 36,
            action: ButtonAction::Copy,
            icon: "📋".to_string(),
            hovered: false,
            is_primary: false,
        });
        x += BUTTON_SIZE + BUTTON_SPACING;

        self.buttons.push(ToolButton {
            x, y, width: BUTTON_SIZE, height: 36,
            action: ButtonAction::Cancel,
            icon: "✕".to_string(),
            hovered: false,
            is_primary: false,
        });
    }

    pub unsafe fn show(&mut self) {
        TOOLBAR_INSTANCE = Some(self as *mut Toolbar);
        ShowWindow(self.hwnd, SW_SHOW);
        UpdateWindow(self.hwnd).ok();
    }

    unsafe fn on_paint(&self) {
        let mut ps = PAINTSTRUCT::default();
        let hdc = BeginPaint(self.hwnd, &mut ps);

        // 绘制背景
        let brush = CreateSolidBrush(COLORREF(0x2E2A2E));
        let rect = RECT { left: 0, top: 0, right: TOOLBAR_WIDTH, bottom: TOOLBAR_HEIGHT };
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);

        // 绘制按钮
        for btn in &self.buttons {
            self.draw_button(hdc, btn);
        }

        EndPaint(self.hwnd, &ps);
    }

    unsafe fn draw_button(&self, hdc: HDC, btn: &ToolButton) {
        // 按钮背景色
        let bg_color = if btn.is_primary {
            // 保存按钮蓝色
            if btn.hovered {
                COLORREF(0x1884E4)
            } else {
                COLORREF(0x0078D7)
            }
        } else if btn.hovered {
            COLORREF(0x464248)
        } else if let ButtonAction::Tool(tool) = btn.action {
            if tool == self.selected_tool {
                COLORREF(0x2878DC) // 选中
            } else {
                COLORREF(0x3C3C3C)
            }
        } else {
            COLORREF(0x3C3C3C)
        };

        let brush = CreateSolidBrush(bg_color);
        let rect = RECT {
            left: btn.x,
            top: btn.y,
            right: btn.x + btn.width,
            bottom: btn.y + btn.height,
        };
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);

        // 绘制图标
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COLORREF(0xFFFFFF));

        let icon_wide: Vec<u16> = btn.icon.encode_utf16().chain(std::iter::once(0)).collect();
        let mut icon_rect = rect.clone();
        DrawTextW(hdc, &icon_wide, &mut icon_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    unsafe fn on_mouse_move(&mut self, x: i32, y: i32) {
        let mut changed = false;
        for btn in &mut self.buttons {
            let hovered = x >= btn.x && x < btn.x + btn.width && y >= btn.y && y < btn.y + btn.height;
            if btn.hovered != hovered {
                btn.hovered = hovered;
                changed = true;
            }
        }
        if changed {
            InvalidateRect(self.hwnd, None, FALSE);
        }
    }

    unsafe fn on_click(&mut self, x: i32, y: i32) {
        for btn in &self.buttons {
            if x >= btn.x && x < btn.x + btn.width && y >= btn.y && y < btn.y + btn.height {
                match btn.action {
                    ButtonAction::Tool(tool) => {
                        self.selected_tool = if self.selected_tool == tool {
                            ToolMode::None
                        } else {
                            tool
                        };
                        InvalidateRect(self.hwnd, None, FALSE);
                    }
                    ButtonAction::Save => {
                        // 发送消息给父窗口
                        SendMessageW(self.parent, WM_TOOLBAR_ACTION, WPARAM(1), LPARAM(0));
                    }
                    ButtonAction::Copy => {
                        SendMessageW(self.parent, WM_TOOLBAR_ACTION, WPARAM(2), LPARAM(0));
                    }
                    ButtonAction::Cancel => {
                        SendMessageW(self.parent, WM_TOOLBAR_ACTION, WPARAM(3), LPARAM(0));
                    }
                }
                return;
            }
        }
    }
}

unsafe extern "system" fn toolbar_wndproc(
    hwnd: HWND,
    msg: u32,
    wparam: WPARAM,
    lparam: LPARAM,
) -> LRESULT {
    if let Some(toolbar_ptr) = TOOLBAR_INSTANCE {
        let toolbar = &mut *toolbar_ptr;

        match msg {
            WM_PAINT => {
                toolbar.on_paint();
                return LRESULT(0);
            }
            WM_MOUSEMOVE => {
                let x = (lparam.0 & 0xFFFF) as i16 as i32;
                let y = ((lparam.0 >> 16) & 0xFFFF) as i16 as i32;
                toolbar.on_mouse_move(x, y);
                return LRESULT(0);
            }
            WM_LBUTTONDOWN => {
                let x = (lparam.0 & 0xFFFF) as i16 as i32;
                let y = ((lparam.0 >> 16) & 0xFFFF) as i16 as i32;
                toolbar.on_click(x, y);
                return LRESULT(0);
            }
            _ => {}
        }
    }

    DefWindowProcW(hwnd, msg, wparam, lparam)
}
