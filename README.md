# ModernScreenshot

ModernScreenshot is an ultra-lightweight, high-performance Windows screenshot tool built with modern C++17 and Win32 / GDI / GDI+. It features a custom-drawn selection overlay, a PixPin / Snip-style vector edit toolbar, automated scrollable long screenshots, and a global hotkey (`Ctrl+1`).

The implementation is intentionally small and zero-dependency:

- **Windows Native**: Win32 / GDI / GDI+ only
- **Zero Bloat**: No Electron, Qt, GTK, or external runtime dependencies
- **Low Footprint**: Idle memory footprint under 5 MB RSS
- **Vector Annotations**: Stored as crisp vector operations until export
- **Standalone Portable**: Single executable binary with embedded High-DPI manifest

## Features

- **Global Hotkey**: Default `Ctrl+1`, configurable to any key or modifier combination
- **System Tray**: Custom-drawn dark popup menu with Capture, Long Capture, Settings, and Exit
- **Region Selection Overlay**: Fullscreen dark mask with pixel dimension badge and mode toggle
- **Long Screenshot Mode**: Automatic window scrolling, settle delay, seam template matching, and seamless stitching
- **PixPin-Style Vector Editor**:
  - `Blur` / Mosaic for sensitive information
  - `Arrow` with smooth antialiased arrowheads
  - `Box` (Rectangle outline)
  - `Text` with inline editing
  - `Undo` (`Ctrl+Z`)
  - `Copy` (`Ctrl+C`) writes directly to clipboard (`CF_BITMAP` and DIB)
  - `Save` (`Ctrl+S`) saves to `%USERPROFILE%\Pictures\Screenshots\`
  - Drag the empty toolbar area to reposition the editor
  - Mouse wheel, `PageUp`, `PageDown`, `Home`, and `End` navigation for tall/long screenshots
- **Settings & Autostart**:
  - Custom dark settings window
  - Interactive hotkey recorder
  - Hotkey conflict warning
  - Toggle "Start with Windows" via `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
  - Persisted in `%APPDATA%\ModernScreenshot\settings.ini`
- **Single Instance**: Wakes existing instance on duplicate launch

## Build

Requires MSVC (`cl.exe`) with C++17 support:

```powershell
.\scripts\build-windows.ps1 -Configuration Release
```

The executable is compiled to `build\windows\ModernScreenshot.exe`, and the distribution package is generated at `build\windows\ModernScreenshot-windows-x64.zip`.

GitHub Actions automatically builds, smoke-tests, and publishes release packages on every push to `master`.

## Run

```powershell
.\build\windows\ModernScreenshot.exe
```

When started, ModernScreenshot minimizes to the system notification area (system tray). Press the configured hotkey (default `Ctrl+1`), drag a region, and release to begin editing.

### Command Line Options

- One-shot capture:
  ```powershell
  .\build\windows\ModernScreenshot.exe --once
  ```

- Start long screenshot directly:
  ```powershell
  .\build\windows\ModernScreenshot.exe --long
  ```

- Check version:
  ```powershell
  .\build\windows\ModernScreenshot.exe --version
  ```

- Automated export self-test (used in CI):
  ```powershell
  .\build\windows\ModernScreenshot.exe --self-test output.png
  ```

## Architecture

The project is structured into clean, decoupled C++17 modules:

- `src/common.h`: Shared constants, enumerations, geometry helpers, and state structures
- `src/raii.h`: Zero-cost RAII resource guards for Win32 GDI handles (`ScopedDC`, `ScopedGdiObject`, `ScopedSelectObject`)
- `src/utils.h` / `src/utils.cpp`: Configuration, registry startup, screenshot path, and clipboard handlers
- `src/graphics.h` / `src/graphics.cpp`: Screen capture, GDI+ PNG export, mosaic pixelation, and vector drawing
- `src/overlay.h` / `src/overlay.cpp`: Fullscreen selection overlay and mode pill controls
- `src/long_capture.h` / `src/long_capture.cpp`: Long screenshot scrolling, frame correlation, and template stitching engine
- `src/editor.h` / `src/editor.cpp`: Floating PixPin-style editor window, annotation rendering, and undo stack
- `src/tray_popup.h` / `src/tray_popup.cpp`: Custom-drawn dark popup menu for the system tray
- `src/settings.h` / `src/settings.cpp`: Custom-drawn dark settings dialog and hotkey capture
- `src/app.h` / `src/app.cpp`: Application lifecycle, single-instance management, and daemon message loop
- `src/main.cpp`: Entry point (`wWinMain`) and CLI argument dispatch
- `src/modern_screenshot.manifest`: Per-Monitor V2 High-DPI and Common Controls v6 manifest

## License

MIT License. See [LICENSE](LICENSE) for details.
