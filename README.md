# ModernScreenshot

ModernScreenshot is a lightweight Linux/X11 screenshot daemon with a custom-drawn selection UI and the global hotkey `Ctrl+1`.

The implementation is intentionally small:

- C11, Xlib, and Xrender only
- no Electron, Qt, GTK, libpng, zlib, or tray runtime
- idle RSS target below 5 MB
- screenshot capture uses a fixed tile buffer instead of storing a full-screen image in memory
- PNG output is written as valid uncompressed PNG data to keep memory predictable

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

Press `Ctrl+1`, drag a region, and release the mouse. PNG files are saved to:

```text
~/Pictures/Screenshots/
```

For a one-shot capture:

```sh
./build/modern-screenshot --once
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
