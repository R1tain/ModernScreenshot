use windows::{
    Win32::Graphics::Gdi::*,
    Win32::UI::WindowsAndMessaging::*,
};

pub struct Screenshot {
    pub width: i32,
    pub height: i32,
    pub hdc: HDC,
    pub hbitmap: HBITMAP,
}

impl Screenshot {
    pub unsafe fn capture() -> Result<Self, Box<dyn std::error::Error>> {
        // 获取屏幕 DC
        let screen_dc = GetDC(None);

        // 获取屏幕尺寸
        let width = GetSystemMetrics(SM_CXSCREEN);
        let height = GetSystemMetrics(SM_CYSCREEN);

        // 创建内存 DC
        let mem_dc = CreateCompatibleDC(screen_dc);

        // 创建位图
        let hbitmap = CreateCompatibleBitmap(screen_dc, width, height);

        // 选择位图到内存 DC
        SelectObject(mem_dc, hbitmap);

        // 复制屏幕到内存
        BitBlt(
            mem_dc,
            0, 0,
            width, height,
            screen_dc,
            0, 0,
            SRCCOPY,
        );

        // 释放屏幕 DC
        ReleaseDC(None, screen_dc);

        Ok(Self {
            width,
            height,
            hdc: mem_dc,
            hbitmap,
        })
    }

    pub unsafe fn draw_to(&self, target_dc: HDC, x: i32, y: i32, w: i32, h: i32) -> Result<(), Box<dyn std::error::Error>> {
        BitBlt(
            target_dc,
            x, y, w, h,
            self.hdc,
            x, y,
            SRCCOPY,
        );
        Ok(())
    }
}

impl Drop for Screenshot {
    fn drop(&mut self) {
        unsafe {
            DeleteObject(self.hbitmap);
            DeleteDC(self.hdc);
        }
    }
}
