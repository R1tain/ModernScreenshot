use windows::{
    core::*,
    Win32::Foundation::*,
    Win32::Graphics::Gdi::*,
    Win32::UI::WindowsAndMessaging::*,
    Win32::UI::Controls::Dialogs::*,
    Win32::System::Com::*,
    Win32::Storage::FileSystem::*,
};
use std::ptr;

pub unsafe fn save_bitmap_to_file(hdc: HDC, x: i32, y: i32, width: i32, height: i32) -> Result<()> {
    // 创建文件对话框
    let mut filename = [0u16; 260];
    let filter = "PNG Files\0*.png\0All Files\0*.*\0\0";
    let filter_wide: Vec<u16> = filter.encode_utf16().collect();

    let mut ofn = OPENFILENAMEW {
        lStructSize: std::mem::size_of::<OPENFILENAMEW>() as u32,
        hwndOwner: HWND(0),
        lpstrFilter: PCWSTR(filter_wide.as_ptr()),
        lpstrFile: PWSTR(filename.as_mut_ptr()),
        nMaxFile: 260,
        lpstrDefExt: w!("png"),
        Flags: OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST,
        ..Default::default()
    };

    if GetSaveFileNameW(&mut ofn).as_bool() {
        // 创建位图
        let mem_dc = CreateCompatibleDC(hdc);
        let hbitmap = CreateCompatibleBitmap(hdc, width, height)?;
        SelectObject(mem_dc, hbitmap);

        // 复制区域
        BitBlt(mem_dc, 0, 0, width, height, hdc, x, y, SRCCOPY)?;

        // 保存为文件（简化版：使用 BMP 格式）
        save_bitmap_as_bmp(&filename, hbitmap, width, height)?;

        DeleteDC(mem_dc);
        DeleteObject(hbitmap);

        MessageBoxW(
            HWND(0),
            w!("截图已保存！"),
            w!("成功"),
            MB_OK | MB_ICONINFORMATION,
        );
    }

    Ok(())
}

unsafe fn save_bitmap_as_bmp(filename: &[u16], hbitmap: HBITMAP, width: i32, height: i32) -> Result<()> {
    let hdc = GetDC(None);
    let mut bmi = BITMAPINFO {
        bmiHeader: BITMAPINFOHEADER {
            biSize: std::mem::size_of::<BITMAPINFOHEADER>() as u32,
            biWidth: width,
            biHeight: -height, // 负数表示从上到下
            biPlanes: 1,
            biBitCount: 24,
            biCompression: BI_RGB.0 as u32,
            ..Default::default()
        },
        ..Default::default()
    };

    let bytes_per_line = ((width * 3 + 3) / 4) * 4;
    let image_size = bytes_per_line * height;
    let mut pixels = vec![0u8; image_size as usize];

    GetDIBits(
        hdc,
        hbitmap,
        0,
        height as u32,
        Some(pixels.as_mut_ptr() as *mut _),
        &mut bmi,
        DIB_RGB_COLORS,
    );

    // 写入 BMP 文件
    let file_size = 54 + image_size;
    let mut file_header = [0u8; 54];

    // BITMAPFILEHEADER
    file_header[0] = b'B';
    file_header[1] = b'M';
    file_header[2..6].copy_from_slice(&(file_size as u32).to_le_bytes());
    file_header[10..14].copy_from_slice(&54u32.to_le_bytes());

    // BITMAPINFOHEADER
    file_header[14..18].copy_from_slice(&40u32.to_le_bytes());
    file_header[18..22].copy_from_slice(&(width as u32).to_le_bytes());
    file_header[22..26].copy_from_slice(&(height as u32).to_le_bytes());
    file_header[26..28].copy_from_slice(&1u16.to_le_bytes());
    file_header[28..30].copy_from_slice(&24u16.to_le_bytes());

    let h_file = CreateFileW(
        PCWSTR(filename.as_ptr()),
        FILE_GENERIC_WRITE.0,
        FILE_SHARE_NONE,
        None,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        None,
    )?;

    let mut written = 0u32;
    WriteFile(h_file, Some(&file_header), Some(&mut written), None)?;
    WriteFile(h_file, Some(&pixels), Some(&mut written), None)?;

    CloseHandle(h_file)?;
    ReleaseDC(None, hdc);

    Ok(())
}

pub unsafe fn copy_bitmap_to_clipboard(hwnd: HWND, hdc: HDC, x: i32, y: i32, width: i32, height: i32) -> Result<()> {
    // 打开剪贴板
    OpenClipboard(hwnd)?;
    EmptyClipboard()?;

    // 创建位图
    let mem_dc = CreateCompatibleDC(hdc);
    let hbitmap = CreateCompatibleBitmap(hdc, width, height)?;
    SelectObject(mem_dc, hbitmap);

    // 复制区域
    BitBlt(mem_dc, 0, 0, width, height, hdc, x, y, SRCCOPY)?;

    // 设置到剪贴板
    SetClipboardData(CF_BITMAP.0 as u32, HANDLE(hbitmap.0))?;

    CloseClipboard()?;
    DeleteDC(mem_dc);

    MessageBoxW(
        hwnd,
        w!("已复制到剪贴板！"),
        w!("成功"),
        MB_OK | MB_ICONINFORMATION,
    );

    Ok(())
}
