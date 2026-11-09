#pragma once

#include "common.h"
#include "graphics.h"

namespace ModernScreenshot {

struct OverlayState {
    ScreenCapture capture;
    bool showModeButtons = false;
    CaptureKind kind = CaptureRegion;
    int hoverMode = -1;
    bool hasMouse = false;
    POINT mouse = {0, 0};
    bool dragging = false;
    bool done = false;
    bool cancelled = false;
    POINT start = {0, 0};
    POINT current = {0, 0};
    RECT selection = {0, 0, 0, 0};
};

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

bool RunSelectionOverlay(HINSTANCE instance, const ScreenCapture &capture, RECT *selection,
                         CaptureKind *kind, bool showModeButtons, CaptureKind initialKind);

} // namespace ModernScreenshot
