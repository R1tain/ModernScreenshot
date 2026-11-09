#pragma once

#include "common.h"
#include "graphics.h"

namespace ModernScreenshot {

struct LongSessionState {
    RECT selection = {0, 0, 0, 0};
    bool done = false;
    bool cancelled = false;
    bool started = false;
    bool hasPrev = false;
    int w = 0;
    int h = 0;
    int totalH = 0;
    int skippedFrames = 0;
    int lastCopyH = 0;
    int unchangedSteps = 0;
    bool captureLimitReached = false;
    bool lastSampleUnchanged = false;
    HBITMAP pendingBitmap = NULL;
    int pendingStableFrames = 0;
    std::wstring statusText;
    HWND target = NULL;
    HWND targetRoot = NULL;
    std::vector<LongFrame> frames;
    std::vector<BYTE> prevPixels;
    std::vector<BYTE> pendingPixels;
};

LRESULT CALLBACK LongSessionProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

HBITMAP RunLongSession(HINSTANCE instance, const RECT &selection, HWND preferredTarget, int *outH);

} // namespace ModernScreenshot
