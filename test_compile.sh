#!/bin/bash

echo "创建最小测试项目..."

cat > /tmp/test_main.rs << 'RUST_CODE'
use windows::{
    core::*,
    Win32::Foundation::*,
    Win32::UI::WindowsAndMessaging::*,
};

fn main() -> Result<()> {
    unsafe {
        MessageBoxW(
            HWND(0),
            w!("Rust 编译测试成功！"),
            w!("测试"),
            MB_OK,
        );
    }
    Ok(())
}
RUST_CODE

echo "测试代码创建完成"
cat /tmp/test_main.rs
