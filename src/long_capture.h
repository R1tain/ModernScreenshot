#pragma once

#include "common.h"
#include "graphics.h"
#include "raii.h"

namespace ModernScreenshot {

static constexpr int kRowProfileBins = 16;

struct RowSignature {
    int meanLuma = 0;                     // 0..255 average luminance across active width
    int horizGrad = 0;                    // 0..255 horizontal edge/texture energy (high on text/UI, ~0 on uniform background)
    int vertGrad = 0;                     // 0..255 vertical difference vs adjacent row (low on inter-line whitespace)
    unsigned char bins[kRowProfileBins];  // 16-bin horizontal luminance profile across the row
};

struct LongSessionState {
    RECT selection = {0, 0, 0, 0};
    bool done = false;
    bool cancelled = false;
    bool started = false;
    bool hasPrev = false;
    bool keyDebounceDone = false;
    bool pausedByHover = false;
    int hoveredButton = 0; // 0 = none, 1 = Stop, 2 = Cancel
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
    std::vector<RowSignature> prevSignatures;
    int expectedScrollStep = 0;
};

LRESULT CALLBACK LongSessionProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

HBITMAP RunLongSession(HINSTANCE instance, const RECT &selection, HWND preferredTarget, int *outH);

} // namespace ModernScreenshot
