use windows::{
    core::*,
    Win32::Foundation::*,
    Win32::Graphics::Gdi::*,
    Win32::UI::WindowsAndMessaging::*,
    Win32::UI::Input::KeyboardAndMouse::VIRTUAL_KEY,
};

pub const VK_ESCAPE: VIRTUAL_KEY = VIRTUAL_KEY(0x1B);
