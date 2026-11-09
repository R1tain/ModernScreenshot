#pragma once

#include "common.h"
#include "graphics.h"

namespace ModernScreenshot {

enum LongHudHit {
    LongHitNone,
    LongHitAutoToggle,
    LongHitDone,
    LongHitCancel,
};

struct LongSessionState {
    RECT selection = {0, 0, 0, 0};
    bool done = false;
    bool cancelled = false;
    bool started = false;
    bool autoScroll = true;
    bool hasPrev = false;
    int w = 0;
    int h = 0;
    int totalH = 0;
    int skippedFrames = 0;
    int lastCopyH = 0;
    int unchangedSteps = 0;
    bool captureLimitReached = false;
    bool lastSampleUnchanged = false;
    std::wstring statusText;
    HWND target = NULL;
    HWND targetRoot = NULL;
    HWND hudWnd = NULL;
    HWND previewWnd = NULL;
    LongHudHit hover = LongHitNone;
    std::vector<LongFrame> frames;
    std::vector<BYTE> prevPixels;
};

LRESULT CALLBACK LongSessionProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK LongPreviewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

HBITMAP RunLongSession(HINSTANCE instance, const RECT &selection, HWND preferredTarget, int *outH);

} // namespace ModernScreenshot
