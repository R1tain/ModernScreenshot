# ModernScreenshot

ModernScreenshot is a lightweight Linux/X11 screenshot daemon with a custom-drawn selection UI, a PixPin/Snip-style edit toolbar, and the global hotkey `Ctrl+1`.

The implementation is intentionally small:

- C11, Xlib, and Xrender only
- no Electron, Qt, GTK, libpng, zlib, or tray runtime
- idle RSS target below 5 MB
- screenshot capture uses a fixed tile buffer instead of storing a full-screen image in memory
- annotations are stored as vector operations until export
- PNG output is written as valid uncompressed PNG data to keep memory predictable

## Features

- `Ctrl+1` global hotkey
- region selection overlay
- editor toolbar: blur, arrow, rectangle, text, undo, copy, save
- mosaic blur for sensitive content
- `Ctrl+Z`, `Ctrl+C`, and `Ctrl+S` in the editor
- clipboard ownership with `image/png`, text path, and URI list targets

## Build

```sh
sudo apt-get install build-essential pkg-config libx11-dev libxrender-dev
make release
```

The binary is written to `build/modern-screenshot`.

## Run

```sh
./build/modern-screenshot
```

Press `Ctrl+1`, drag a region, and release the mouse. The editor opens on the captured region:

- `Blur`, `->`, `Box`, and `Text` choose the active tool
- `Undo` removes the last annotation
- `Copy` writes a PNG to the X11 clipboard
- `Save` writes a PNG file to:

```text
~/Pictures/Screenshots/
```

For a one-shot capture:

```sh
./build/modern-screenshot --once
```

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

## Notes

This tool targets X11. On Wayland sessions, global hotkeys and root-window capture are intentionally restricted by the compositor, so use an X11 session or XWayland environment that exposes the root window.

The 5 MB target applies to the idle daemon. During copy, X11 clients may request the PNG payload from the clipboard, which can temporarily allocate memory proportional to the exported file size.

Useful next features would be numbered step markers, adjustable stroke width, color swatches, pinned screenshots, and a delayed full-screen capture mode.
