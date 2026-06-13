# ModernScreenshot

ModernScreenshot is a lightweight screenshot daemon with a custom-drawn selection UI, a PixPin/Snip-style edit toolbar, and the global hotkey `Ctrl+1`.

The implementation is intentionally small:

- Linux: C11, Xlib, and Xrender only
- Windows: Win32/GDI/GDI+ only
- no Electron, Qt, GTK, libpng, zlib, or tray runtime
- idle RSS target below 5 MB
- screenshot capture uses a fixed tile buffer instead of storing a full-screen image in memory
- annotations are stored as vector operations until export
- PNG output is written as valid uncompressed PNG data to keep memory predictable

## Features

- `Ctrl+1` global hotkey
- Windows tray icon with Capture, Long Capture, Settings, and Exit
- configurable Windows hotkey stored in `%APPDATA%\ModernScreenshot\settings.ini`
- hotkey conflict warning with a settings entry point
- region selection overlay
- Windows long screenshot mode for scrollable content
- editor toolbar: blur, arrow, rectangle, text, undo, copy, save
- mosaic blur for sensitive content
- `Ctrl+Z`, `Ctrl+C`, and `Ctrl+S` in the editor
- clipboard ownership with `image/png`, text path, and URI list targets

## Build

Linux:

```sh
sudo apt-get install build-essential pkg-config libx11-dev libxrender-dev
make release
```

The binary is written to `build/modern-screenshot`.

Windows:

```powershell
.\scripts\build-windows.ps1 -Configuration Release
```

The executable is written to `build\windows\ModernScreenshot.exe`, and the zip package is written to `build\windows\ModernScreenshot-windows-x64.zip`.

Release builds are published by GitHub Actions to GitHub Releases. Download `ModernScreenshot-windows-x64.zip` from the latest release for Windows.

## Run

Linux:

```sh
./build/modern-screenshot
```

Windows:

```powershell
.\build\windows\ModernScreenshot.exe
```

On Windows, the app stays in the system tray. Use the tray menu to capture, long capture, open settings, or exit. If `Ctrl+1` is already used by another app, open Settings from the tray menu and choose another hotkey.

Press the configured hotkey, drag a region, and release the mouse. The editor opens on the captured region:

- `Blur`, `->`, `Box`, and `Text` choose the active tool
- `Undo` removes the last annotation
- `Copy` writes the image to the clipboard
- `Save` writes a PNG file to:
- drag the empty toolbar area to move the editor window
- use the mouse wheel, `PageUp`, `PageDown`, `Home`, or `End` to browse tall screenshots

```text
~/Pictures/Screenshots/
```

On Windows, saved files go to:

```text
%USERPROFILE%\Pictures\Screenshots\
```

For a one-shot capture:

```sh
./build/modern-screenshot --once
```

For a Windows long screenshot, choose `Long Capture...` from the tray menu or run:

```powershell
.\build\windows\ModernScreenshot.exe --long
```

Drag the visible scrollable content area. The app captures the first frame, scrolls downward, stitches up to 8 frames, and opens the result in the editor. Long capture works best when the selected area is inside a window that responds to the mouse wheel.

For an automated editor export test:

```sh
xvfb-run -a ./build/modern-screenshot --self-test /tmp/modern-screenshot-test.png
```

## Install For Current User

```sh
make PREFIX="$HOME/.local" install
systemctl --user daemon-reload
systemctl --user enable --now modern-screenshot.service
```

## Memory Check

Run this inside an X11 session:

```sh
make memory-check
```

The check fails if idle RSS is above `5120 KB`. You can override the threshold:

```sh
MODERN_SCREENSHOT_RSS_LIMIT_KB=5120 make memory-check
```

## Publish A Release

Create and push a version tag:

```sh
git tag v0.1.0
git push origin v0.1.0
```

The `release` workflow builds Linux and Windows packages, creates a GitHub Release, and uploads:

- `ModernScreenshot-linux-x64.tar.gz`
- `ModernScreenshot-windows-x64.zip`

You can also run the `release` workflow manually from the Actions tab and provide a tag such as `v0.1.0`.

## Notes

This tool targets X11. On Wayland sessions, global hotkeys and root-window capture are intentionally restricted by the compositor, so use an X11 session or XWayland environment that exposes the root window.

The 5 MB target applies to the idle daemon. During copy, export, or long screenshot stitching, the app may temporarily allocate memory proportional to the captured image size.

Useful next features would be numbered step markers, adjustable stroke width, color swatches, pinned screenshots, and a delayed full-screen capture mode.
