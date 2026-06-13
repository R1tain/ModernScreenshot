use windows::{
    Win32::Foundation::*,
    Win32::Graphics::Gdi::*,
};

#[derive(Clone, Copy, PartialEq)]
pub enum ToolMode {
    None,
    Rectangle,
    Arrow,
    Pen,
    Text,
    Mosaic,
}

#[derive(Clone)]
pub struct DrawAction {
    pub tool: ToolMode,
    pub start_x: i32,
    pub start_y: i32,
    pub end_x: i32,
    pub end_y: i32,
    pub color: COLORREF,
    pub width: i32,
    pub text: Option<String>,
}

impl DrawAction {
    pub unsafe fn draw(&self, hdc: HDC) {
        match self.tool {
            ToolMode::Rectangle => self.draw_rectangle(hdc),
            ToolMode::Arrow => self.draw_arrow(hdc),
            ToolMode::Pen => self.draw_pen(hdc),
            ToolMode::Mosaic => self.draw_mosaic(hdc),
            _ => {}
        }
    }

    unsafe fn draw_rectangle(&self, hdc: HDC) {
        let pen = CreatePen(PS_SOLID, self.width, self.color);
        let old_pen = SelectObject(hdc, pen);
        let old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

        Rectangle(hdc, self.start_x, self.start_y, self.end_x, self.end_y);

        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(pen);
    }

    unsafe fn draw_arrow(&self, hdc: HDC) {
        let pen = CreatePen(PS_SOLID, self.width, self.color);
        let old_pen = SelectObject(hdc, pen);

        // 画线
        MoveToEx(hdc, self.start_x, self.start_y, None);
        LineTo(hdc, self.end_x, self.end_y);

        // 画箭头（简化版）
        let dx = (self.end_x - self.start_x) as f32;
        let dy = (self.end_y - self.start_y) as f32;
        let len = (dx * dx + dy * dy).sqrt();

        if len > 0.0 {
            let arrow_len = 15.0;
            let angle = 0.5;

            let ux = dx / len;
            let uy = dy / len;

            let x1 = self.end_x as f32 - arrow_len * (ux + angle * uy);
            let y1 = self.end_y as f32 - arrow_len * (uy - angle * ux);
            let x2 = self.end_x as f32 - arrow_len * (ux - angle * uy);
            let y2 = self.end_y as f32 - arrow_len * (uy + angle * ux);

            MoveToEx(hdc, self.end_x, self.end_y, None);
            LineTo(hdc, x1 as i32, y1 as i32);
            MoveToEx(hdc, self.end_x, self.end_y, None);
            LineTo(hdc, x2 as i32, y2 as i32);
        }

        SelectObject(hdc, old_pen);
        DeleteObject(pen);
    }

    unsafe fn draw_pen(&self, hdc: HDC) {
        let pen = CreatePen(PS_SOLID, self.width, self.color);
        let old_pen = SelectObject(hdc, pen);

        MoveToEx(hdc, self.start_x, self.start_y, None);
        LineTo(hdc, self.end_x, self.end_y);

        SelectObject(hdc, old_pen);
        DeleteObject(pen);
    }

    unsafe fn draw_mosaic(&self, hdc: HDC) {
        // 简化版马赛克：用灰色矩形填充
        let brush = CreateSolidBrush(COLORREF(0x808080));
        let old_brush = SelectObject(hdc, brush);

        let x = self.start_x.min(self.end_x);
        let y = self.start_y.min(self.end_y);
        let w = (self.start_x - self.end_x).abs();
        let h = (self.start_y - self.end_y).abs();

        for i in (0..w).step_by(10) {
            for j in (0..h).step_by(10) {
                let rect = RECT {
                    left: x + i,
                    top: y + j,
                    right: x + i + 8,
                    bottom: y + j + 8,
                };
                FillRect(hdc, &rect, brush);
            }
        }

        SelectObject(hdc, old_brush);
        DeleteObject(brush);
    }
}
