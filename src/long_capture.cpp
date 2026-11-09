#include "long_capture.h"
#include <atomic>
#include <cmath>

namespace ModernScreenshot {

static HHOOK g_mouseHook = NULL;
static std::atomic<bool> g_manualWheelTriggered{false};
static RECT g_hookSelection = {0, 0, 0, 0};

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_MOUSEWHEEL) {
        MSLLHOOKSTRUCT *hookStruct = reinterpret_cast<MSLLHOOKSTRUCT *>(lParam);
        if (hookStruct) {
            POINT pt = hookStruct->pt;
            if (pt.x >= g_hookSelection.left && pt.x <= g_hookSelection.right &&
                pt.y >= g_hookSelection.top && pt.y <= g_hookSelection.bottom) {
                g_manualWheelTriggered.store(true);
            }
        }
    }
    return CallNextHookEx(g_mouseHook, nCode, wParam, lParam);
}

static bool IsOwnWindow(HWND hwnd) {
    while (hwnd) {
        wchar_t className[96] = {};
        GetClassNameW(hwnd, className, 96);
        if (wcscmp(className, kHiddenClass) == 0 || wcscmp(className, kOverlayClass) == 0 ||
            wcscmp(className, kLongSessionClass) == 0 || wcscmp(className, kLongPreviewClass) == 0 ||
            wcscmp(className, kEditorClass) == 0 || wcscmp(className, kSettingsClass) == 0) {
            return true;
        }
        hwnd = GetParent(hwnd);
    }
    return false;
}

static HWND WindowAtPointForScroll(POINT point, HWND fallback) {
    HWND hwnd = WindowFromPoint(point);
    if (hwnd && !IsOwnWindow(hwnd)) {
        return hwnd;
    }
    return fallback;
}

static void FocusLongTarget(LongSessionState *state) {
    if (!state || !state->targetRoot || !IsWindow(state->targetRoot)) {
        return;
    }
    if (IsIconic(state->targetRoot)) {
        ShowWindow(state->targetRoot, SW_RESTORE);
    }
    SetForegroundWindow(state->targetRoot);
}

static bool SendLongScrollStep(LongSessionState *state) {
    if (!state || !state->target || !IsWindow(state->target)) {
        return false;
    }
    POINT center = {state->selection.left + state->w / 2, state->selection.top + state->h / 2};
    FocusLongTarget(state);
    SetCursorPos(center.x, center.y);
    HWND pointTarget = WindowAtPointForScroll(center, state->target);
    if (pointTarget && IsWindow(pointTarget)) {
        state->target = pointTarget;
        HWND root = RootWindow(pointTarget);
        if (root && !IsOwnWindow(root)) {
            state->targetRoot = root;
        }
    }
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(-WHEEL_DELTA);
    return SendInput(1, &input, sizeof(input)) == 1;
}

static void PumpMessagesFor(HWND hwnd, int milliseconds) {
    DWORD end = GetTickCount() + static_cast<DWORD>(milliseconds);
    MSG msg;
    do {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(10);
    } while (GetTickCount() < end && IsWindow(hwnd));
}

static bool AppendLongFrameStrip(LongSessionState *state, HBITMAP bitmap, int overlap) {
    if (!state || !bitmap) return false;
    if (state->totalH >= kLongCaptureMaxHeight ||
        static_cast<int>(state->frames.size()) >= kLongCaptureMaxFrames) {
        state->captureLimitReached = true;
        return false;
    }

    int copyH = state->h - overlap;
    if (state->totalH + copyH > kLongCaptureMaxHeight) {
        copyH = kLongCaptureMaxHeight - state->totalH;
    }
    if (copyH <= 0) return false;

    HBITMAP strip = CopyBitmapStrip(bitmap, state->w, overlap, copyH);
    if (!strip) return false;
    state->frames.push_back({strip, copyH});
    state->totalH += copyH;
    state->lastCopyH = copyH;
    state->skippedFrames = 0;
    return true;
}

static inline unsigned int PixelAt(const std::vector<BYTE> &pixels, int w, int x, int y) {
    size_t index = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4;
    return static_cast<unsigned int>(pixels[index]) |
           (static_cast<unsigned int>(pixels[index + 1]) << 8) |
           (static_cast<unsigned int>(pixels[index + 2]) << 16);
}

static inline int PixelAbsDiff(unsigned int a, unsigned int b) {
    int db = static_cast<int>(a & 0xff) - static_cast<int>(b & 0xff);
    int dg = static_cast<int>((a >> 8) & 0xff) - static_cast<int>((b >> 8) & 0xff);
    int dr = static_cast<int>((a >> 16) & 0xff) - static_cast<int>((b >> 16) & 0xff);
    return std::abs(db) + std::abs(dg) + std::abs(dr);
}

static int AverageFrameDiff(const std::vector<BYTE> &a, const std::vector<BYTE> &b, int w, int h) {
    int xStep = std::max(1, w / 32);
    int yStep = std::max(1, h / 32);
    long long diff = 0;
    int samples = 0;
    for (int y = 0; y < h; y += yStep) {
        for (int x = 0; x < w; x += xStep) {
            diff += PixelAbsDiff(PixelAt(a, w, x, y), PixelAt(b, w, x, y));
            ++samples;
        }
    }
    return samples > 0 ? static_cast<int>(diff / samples) : 255;
}

static bool IsSameFramePixels(const std::vector<BYTE> &a, const std::vector<BYTE> &b) {
    return a.size() == b.size() &&
           (a.empty() || std::memcmp(a.data(), b.data(), a.size()) == 0);
}

// Robust Industrial-Grade Multi-Template Seam Matching Algorithm
// Excludes moving scrollbars on the right margin & sticky headers on top
static int MatchTemplateStrip(const std::vector<BYTE> &prev, const std::vector<BYTE> &curr,
                              int w, int h, int templateY, int templateH, int scanStartX, int scanW,
                              int searchMinY, int searchMaxY, int *outError) {
    int xStep = std::max(1, scanW / 64);
    int bestY = -1;
    int bestError = 255 * 3;

    for (int candY = searchMinY; candY <= searchMaxY; ++candY) {
        long long diffSum = 0;
        int samples = 0;

        for (int row = 0; row < templateH; row += 2) {
            int prevRowY = templateY + row;
            int currRowY = candY + row;
            for (int col = scanStartX; col < scanStartX + scanW; col += xStep) {
                unsigned int p1 = PixelAt(prev, w, col, prevRowY);
                unsigned int p2 = PixelAt(curr, w, col, currRowY);
                diffSum += PixelAbsDiff(p1, p2);
                ++samples;
            }
        }

        if (samples > 0) {
            int avgDiff = static_cast<int>(diffSum / samples);
            if (avgDiff < bestError) {
                bestError = avgDiff;
                bestY = candY;
                if (bestError <= 4) break; // Early exit on perfect match
            }
        }
    }

    if (outError) *outError = bestError;
    return bestY;
}

static int EstimateScrollShiftRobust(const std::vector<BYTE> &prev, const std::vector<BYTE> &curr,
                                    int w, int h, int *outError) {
    if (w <= 16 || h <= 32) {
        if (outError) *outError = 255 * 3;
        return 0;
    }

    // Exclude scrollbar margin on right and window edge on left
    int marginR = std::min(24, w / 6);
    int marginL = 4;
    int scanW = w - marginL - marginR;
    if (scanW <= 16) {
        marginL = 0;
        marginR = 0;
        scanW = w;
    }

    const int TH = 32;
    // Primary template placed near bottom of previous frame (above status bars)
    int y1_ref = h - TH - 16;
    if (y1_ref < 0) y1_ref = 0;

    int maxSearchY = y1_ref - 2;
    if (maxSearchY < 0) {
        if (outError) *outError = 255 * 3;
        return 0;
    }

    int t1Error = 255 * 3;
    int y1_best = MatchTemplateStrip(prev, curr, w, h, y1_ref, TH, marginL, scanW,
                                     0, maxSearchY, &t1Error);

    if (y1_best < 0 || t1Error > kLongOverlapErrorLimit) {
        if (outError) *outError = t1Error;
        return 0;
    }

    int shift = y1_ref - y1_best;

    // Cross-verify with secondary template from upper area to eliminate false positives
    int y2_ref = y1_ref - 48;
    if (y2_ref >= 0) {
        int y2_expected = y2_ref - shift;
        if (y2_expected >= 0) {
            int t2Error = 255 * 3;
            MatchTemplateStrip(prev, curr, w, h, y2_ref, TH, marginL, scanW,
                               std::max(0, y2_expected - 2), std::min(h - TH, y2_expected + 2),
                               &t2Error);
            if (t2Error > kLongOverlapErrorLimit + 12) {
                // Secondary template verification failed (likely a repeating pattern)
                if (outError) *outError = t2Error;
                return 0;
            }
        }
    }

    if (outError) *outError = t1Error;
    return shift;
}

static bool AddLongFrame(LongSessionState *state, HBITMAP bitmap, std::vector<BYTE> *pixels) {
    if (!state || !bitmap || !pixels) return false;
    state->lastSampleUnchanged = false;

    if (!state->hasPrev) {
        state->frames.push_back({bitmap, state->h});
        state->prevPixels.swap(*pixels);
        state->totalH = state->h;
        state->lastCopyH = state->h;
        state->hasPrev = true;
        return true;
    }

    if (IsSameFramePixels(state->prevPixels, *pixels)) {
        state->lastSampleUnchanged = true;
        DeleteObject(bitmap);
        return false;
    }

    int matchError = 255 * 3;
    int shift = EstimateScrollShiftRobust(state->prevPixels, *pixels, state->w, state->h, &matchError);
    if (shift <= 0) {
        if (AverageFrameDiff(state->prevPixels, *pixels, state->w, state->h) <= 1) {
            state->lastSampleUnchanged = true;
        } else {
            ++state->skippedFrames;
        }
        DeleteObject(bitmap);
        return false;
    }

    int overlap = state->h - shift;
    bool appended = AppendLongFrameStrip(state, bitmap, overlap);
    DeleteObject(bitmap);
    if (!appended) return false;

    state->prevPixels.swap(*pixels);
    return true;
}

static bool CaptureLongSelectionPixels(LongSessionState *state, HBITMAP *outBitmap, std::vector<BYTE> *outPixels) {
    if (!state || !outBitmap || !outPixels) return false;
    HBITMAP bitmap = CaptureScreenRect(state->selection);
    std::vector<BYTE> pixels;
    if (!bitmap || !ReadBitmapPixels(bitmap, state->w, state->h, &pixels)) {
        if (bitmap) DeleteObject(bitmap);
        return false;
    }
    *outBitmap = bitmap;
    outPixels->swap(pixels);
    return true;
}

static bool SampleLongSelection(LongSessionState *state) {
    HBITMAP bitmap = NULL;
    std::vector<BYTE> pixels;
    if (!CaptureLongSelectionPixels(state, &bitmap, &pixels)) return false;
    return AddLongFrame(state, bitmap, &pixels);
}

static bool SampleStableLongSelection(LongSessionState *state, HWND hwnd) {
    HBITMAP currentBitmap = NULL;
    std::vector<BYTE> currentPixels;
    if (!CaptureLongSelectionPixels(state, &currentBitmap, &currentPixels)) return false;

    for (int attempt = 0; attempt < kLongScrollSettleAttempts && IsWindow(hwnd); ++attempt) {
        PumpMessagesFor(hwnd, kLongScrollSettleMs);
        if (state->done || state->cancelled || !IsWindow(hwnd)) {
            DeleteObject(currentBitmap);
            return false;
        }

        HBITMAP nextBitmap = NULL;
        std::vector<BYTE> nextPixels;
        if (!CaptureLongSelectionPixels(state, &nextBitmap, &nextPixels)) {
            DeleteObject(currentBitmap);
            return false;
        }

        int diff = AverageFrameDiff(currentPixels, nextPixels, state->w, state->h);
        DeleteObject(currentBitmap);
        currentBitmap = nextBitmap;
        currentPixels.swap(nextPixels);
        if (diff <= 1) {
            return AddLongFrame(state, currentBitmap, &currentPixels);
        }
    }

    return AddLongFrame(state, currentBitmap, &currentPixels);
}

static HBITMAP StitchLongFrames(const std::vector<LongFrame> &frames, int w, int *outH) {
    int totalH = 0;
    for (const LongFrame &frame : frames) totalH += frame.copyH;
    if (frames.empty() || w <= 0 || totalH <= 0) return NULL;

    HDC screen = GetDC(NULL);
    HDC source = CreateCompatibleDC(screen);
    HDC dest = CreateCompatibleDC(screen);
    HBITMAP output = CreateCompatibleBitmap(screen, w, totalH);
    if (!source || !dest || !output) {
        if (output) DeleteObject(output);
        if (source) DeleteDC(source);
        if (dest) DeleteDC(dest);
        ReleaseDC(NULL, screen);
        return NULL;
    }

    HGDIOBJ oldDest = SelectObject(dest, output);
    int y = 0;
    for (const LongFrame &frame : frames) {
        HGDIOBJ oldSource = SelectObject(source, frame.bitmap);
        BitBlt(dest, 0, y, w, frame.copyH, source, 0, 0, SRCCOPY);
        SelectObject(source, oldSource);
        y += frame.copyH;
    }

    SelectObject(dest, oldDest);
    DeleteDC(source);
    DeleteDC(dest);
    ReleaseDC(NULL, screen);
    if (outH) *outH = totalH;
    return output;
}

// HUD Button Geometry
static RECT LongHudAutoToggleRect() {
    return {144, 6, 234, 40};
}

static RECT LongHudDoneRect() {
    return {240, 6, 320, 40};
}

static RECT LongHudCancelRect() {
    return {326, 6, 372, 40};
}

static LongHudHit HitTestLongHud(int x, int y) {
    if (PointInRect(x, y, LongHudAutoToggleRect())) return LongHitAutoToggle;
    if (PointInRect(x, y, LongHudDoneRect())) return LongHitDone;
    if (PointInRect(x, y, LongHudCancelRect())) return LongHitCancel;
    return LongHitNone;
}

static void CalculateLongHudPosition(const RECT &selection, int *outX, int *outY) {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    int w = RectWidth(selection);
    int x = selection.left + (w - kLongHudW) / 2;
    x = Clamp(x, vx + 10, vx + vw - kLongHudW - 10);

    int y = selection.bottom + 12;
    if (y + kLongHudH > vy + vh - 12) {
        y = selection.top - kLongHudH - 12;
    }
    y = Clamp(y, vy + 10, vy + vh - kLongHudH - 10);

    *outX = x;
    *outY = y;
}

static void CalculateLongPreviewPosition(const RECT &selection, int *outX, int *outY) {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    int x = selection.right + 12;
    if (x + kLongPreviewW > vx + vw - 8) {
        x = selection.left - kLongPreviewW - 12;
    }
    x = Clamp(x, vx + 8, vx + vw - kLongPreviewW - 8);

    int y = Clamp(selection.top, vy + 8, vy + vh - kLongPreviewH - 8);

    *outX = x;
    *outY = y;
}

static void DrawLongHud(HWND hwnd, LongSessionState *state, HDC dc) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    FillRectColor(dc, rect, RGB(13, 15, 18));

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        // Obsidian Glass Capsule
        GraphicsPath path;
        BuildRoundedRectPath(&path, 0.0f, 0.0f, static_cast<float>(RectWidth(rect)),
                             static_cast<float>(RectHeight(rect)), 12.0f);
        SolidBrush bg(Color(245, 13, 15, 18));
        Pen border(Color(220, 99, 102, 241), 1.2f);
        graphics.FillPath(&bg, &path);
        graphics.DrawPath(&border, &path);

        // Status indicator dot
        Color dotColor = state->autoScroll ? Color(255, 16, 185, 129) : Color(255, 56, 189, 248);
        SolidBrush dotBrush(dotColor);
        graphics.FillEllipse(&dotBrush, 14.0f, 18.0f, 9.0f, 9.0f);

        // Live height & frames text
        wchar_t status[64];
        wsprintfW(status, L"%d px (%d f)", state->totalH, static_cast<int>(state->frames.size()));
        Font font(L"Segoe UI", 13.0f, FontStyleBold, UnitPixel);
        SolidBrush textBrush(Color(255, 248, 250, 252));
        graphics.DrawString(status, -1, &font, PointF(28.0f, 14.0f), &textBrush);

        // Auto Scroll Toggle Button
        RECT autoRect = LongHudAutoToggleRect();
        bool autoHover = state->hover == LongHitAutoToggle;
        GraphicsPath autoPath;
        BuildRoundedRectPath(&autoPath, static_cast<float>(autoRect.left), static_cast<float>(autoRect.top),
                             static_cast<float>(RectWidth(autoRect)), static_cast<float>(RectHeight(autoRect)), 7.0f);
        SolidBrush autoFill(state->autoScroll ? (autoHover ? Color(255, 45, 52, 74) : Color(255, 34, 40, 58))
                                             : (autoHover ? Color(255, 38, 44, 60) : Color(255, 24, 28, 40)));
        Pen autoBorder(state->autoScroll ? Color(255, 129, 140, 248) : Color(255, 58, 66, 86), 1.0f);
        graphics.FillPath(&autoFill, &autoPath);
        graphics.DrawPath(&autoBorder, &autoPath);

        const wchar_t *autoLabel = state->autoScroll ? L"⏸ Pause" : L"▶ Auto";
        Font autoFont(L"Segoe UI", 11.0f, FontStyleRegular, UnitPixel);
        SolidBrush autoTextBrush(Color(255, 226, 232, 240));
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        RectF aTarget(static_cast<float>(autoRect.left), static_cast<float>(autoRect.top),
                      static_cast<float>(RectWidth(autoRect)), static_cast<float>(RectHeight(autoRect)));
        graphics.DrawString(autoLabel, -1, &autoFont, aTarget, &fmt, &autoTextBrush);

        // Finish / Done Button (Electric Indigo)
        RECT doneRect = LongHudDoneRect();
        bool doneHover = state->hover == LongHitDone;
        GraphicsPath donePath;
        BuildRoundedRectPath(&donePath, static_cast<float>(doneRect.left), static_cast<float>(doneRect.top),
                             static_cast<float>(RectWidth(doneRect)), static_cast<float>(RectHeight(doneRect)), 7.0f);
        SolidBrush doneFill(doneHover ? Color(255, 129, 140, 248) : Color(255, 99, 102, 241));
        graphics.FillPath(&doneFill, &donePath);

        Font doneFont(L"Segoe UI", 12.0f, FontStyleBold, UnitPixel);
        SolidBrush doneText(Color(255, 255, 255, 255));
        RectF dTarget(static_cast<float>(doneRect.left), static_cast<float>(doneRect.top),
                      static_cast<float>(RectWidth(doneRect)), static_cast<float>(RectHeight(doneRect)));
        graphics.DrawString(L"✓ Finish", -1, &doneFont, dTarget, &fmt, &doneText);

        // Cancel Button
        RECT cancelRect = LongHudCancelRect();
        bool cancelHover = state->hover == LongHitCancel;
        GraphicsPath cancelPath;
        BuildRoundedRectPath(&cancelPath, static_cast<float>(cancelRect.left), static_cast<float>(cancelRect.top),
                             static_cast<float>(RectWidth(cancelRect)), static_cast<float>(RectHeight(cancelRect)), 7.0f);
        SolidBrush cancelFill(cancelHover ? Color(255, 45, 52, 68) : Color(255, 26, 30, 42));
        Pen cancelBorder(Color(255, 58, 66, 86), 1.0f);
        graphics.FillPath(&cancelFill, &cancelPath);
        graphics.DrawPath(&cancelBorder, &cancelPath);

        Font cancelFont(L"Segoe UI", 12.0f, FontStyleRegular, UnitPixel);
        SolidBrush cancelText(Color(255, 148, 163, 184));
        RectF cTarget(static_cast<float>(cancelRect.left), static_cast<float>(cancelRect.top),
                      static_cast<float>(RectWidth(cancelRect)), static_cast<float>(RectHeight(cancelRect)));
        graphics.DrawString(L"✕", -1, &cancelFont, cTarget, &fmt, &cancelText);
    } else {
        FillRectColor(dc, LongHudDoneRect(), RGB(99, 102, 241));
        DrawCenteredText(dc, L"Finish", LongHudDoneRect(), -12, FW_BOLD, RGB(255, 255, 255));
    }
}

static void DrawLongPreviewWindow(HWND hwnd, LongSessionState *state, HDC dc) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    FillRectColor(dc, rect, RGB(13, 15, 18));

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        GraphicsPath path;
        BuildRoundedRectPath(&path, 0.0f, 0.0f, static_cast<float>(RectWidth(rect)),
                             static_cast<float>(RectHeight(rect)), 8.0f);
        SolidBrush bg(Color(240, 13, 15, 18));
        Pen border(Color(180, 48, 56, 76), 1.0f);
        graphics.FillPath(&bg, &path);
        graphics.DrawPath(&border, &path);

        Font titleFont(L"Segoe UI", 9.0f, FontStyleBold, UnitPixel);
        SolidBrush titleBrush(Color(255, 148, 163, 184));
        graphics.DrawString(L"PREVIEW", -1, &titleFont, PointF(8.0f, 6.0f), &titleBrush);
    }

    if (!state || state->frames.empty() || state->w <= 0 || state->totalH <= 0) {
        return;
    }

    int previewTop = 22;
    int previewW = RectWidth(rect) - 12;
    int previewH = RectHeight(rect) - previewTop - 8;
    if (previewW <= 0 || previewH <= 0) return;

    int drawX = 6;
    int totalScaledH = std::max(1, state->totalH * previewW / state->w);
    int visibleTop = std::max(0, totalScaledH - previewH);

    HDC source = CreateCompatibleDC(dc);
    if (!source) return;

    int scaledY = 0;
    for (const LongFrame &frame : state->frames) {
        int frameScaledH = std::max(1, frame.copyH * previewW / state->w);
        int destTop = previewTop + scaledY - visibleTop;
        int destBottom = destTop + frameScaledH;
        if (destBottom > previewTop && destTop < previewTop + previewH) {
            int clippedTop = std::max(destTop, previewTop);
            int clippedBottom = std::min(destBottom, previewTop + previewH);
            int clippedH = clippedBottom - clippedTop;
            int srcY = std::max(0, (clippedTop - destTop) * frame.copyH / frameScaledH);
            int srcH = std::max(1, clippedH * frame.copyH / frameScaledH);
            HGDIOBJ oldSource = SelectObject(source, frame.bitmap);
            SetStretchBltMode(dc, HALFTONE);
            StretchBlt(dc, drawX, clippedTop, previewW, clippedH,
                       source, 0, srcY, state->w, srcH, SRCCOPY);
            SelectObject(source, oldSource);
        }
        scaledY += frameScaledH;
    }

    DeleteDC(source);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        // Latest seam line indicator
        Pen seamPen(Color(255, 99, 102, 241), 1.5f);
        int indicatorY = previewTop + std::min(previewH - 1, totalScaledH);
        graphics.DrawLine(&seamPen, drawX, indicatorY, drawX + previewW, indicatorY);
    }
}

LRESULT CALLBACK LongPreviewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LongSessionState *state = reinterpret_cast<LongSessionState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        state = reinterpret_cast<LongSessionState *>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            DrawLongPreviewWindow(hwnd, state, dc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK LongSessionProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LongSessionState *state = reinterpret_cast<LongSessionState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        state = reinterpret_cast<LongSessionState *>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_MOUSEMOVE: {
        if (state) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            LongHudHit hit = HitTestLongHud(x, y);
            if (hit != state->hover) {
                state->hover = hit;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (state && state->hover != LongHitNone) {
            state->hover = LongHitNone;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if ((wParam == VK_RETURN || wParam == VK_SPACE) && state) {
            state->done = true;
            return 0;
        }
        if (wParam == VK_ESCAPE && state) {
            state->cancelled = true;
            state->done = true;
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (state) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            LongHudHit hit = HitTestLongHud(x, y);
            if (hit == LongHitAutoToggle) {
                state->autoScroll = !state->autoScroll;
                InvalidateRect(hwnd, NULL, FALSE);
            } else if (hit == LongHitDone) {
                state->done = true;
            } else if (hit == LongHitCancel) {
                state->cancelled = true;
                state->done = true;
            } else {
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            DrawLongHud(hwnd, state, dc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

HBITMAP RunLongSession(HINSTANCE instance, const RECT &selection, HWND preferredTarget, int *outH) {
    LongSessionState state;
    state.selection = selection;
    state.w = RectWidth(selection);
    state.h = RectHeight(selection);
    state.totalH = 0;
    state.autoScroll = true;

    POINT center = {selection.left + state.w / 2, selection.top + state.h / 2};
    state.target = WindowAtPointForScroll(center, preferredTarget);
    state.targetRoot = RootWindow(state.target);
    if (!state.targetRoot && preferredTarget && IsWindow(preferredTarget)) {
        state.targetRoot = RootWindow(preferredTarget);
        state.target = preferredTarget;
    }

    if (state.w <= 16 || state.h <= 16 || !SampleLongSelection(&state)) {
        return NULL;
    }

    FocusLongTarget(&state);
    SetCursorPos(center.x, center.y);

    int hudX = 0, hudY = 0;
    CalculateLongHudPosition(selection, &hudX, &hudY);

    HWND hudWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                 kLongSessionClass, L"ModernScreenshot Long HUD",
                                 WS_POPUP, hudX, hudY, kLongHudW, kLongHudH, NULL, NULL, instance, &state);
    if (!hudWnd) {
        for (LongFrame &frame : state.frames) DeleteObject(frame.bitmap);
        return NULL;
    }
    state.hudWnd = hudWnd;

    int prevX = 0, prevY = 0;
    CalculateLongPreviewPosition(selection, &prevX, &prevY);

    HWND previewWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                     kLongPreviewClass, L"ModernScreenshot Long Preview",
                                     WS_POPUP, prevX, prevY, kLongPreviewW, kLongPreviewH, NULL, NULL, instance, &state);
    state.previewWnd = previewWnd;

    ShowWindow(hudWnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hudWnd);

    if (previewWnd) {
        ShowWindow(previewWnd, SW_SHOWNOACTIVATE);
        UpdateWindow(previewWnd);
    }

    FocusLongTarget(&state);
    SetCursorPos(center.x, center.y);

    // Setup mouse hook for manual wheel scrolling
    g_hookSelection = selection;
    g_manualWheelTriggered.store(false);
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, instance, 0);

    state.started = true;
    state.statusText = L"Auto scrolling...";
    InvalidateRect(hudWnd, NULL, FALSE);
    PumpMessagesFor(hudWnd, 120);

    DWORD lastAutoScrollTick = GetTickCount();

    while (!state.done && !state.cancelled && IsWindow(hudWnd)) {
        if (state.totalH >= kLongCaptureMaxHeight ||
            static_cast<int>(state.frames.size()) >= kLongCaptureMaxFrames) {
            state.captureLimitReached = true;
            break;
        }

        bool shouldScroll = false;
        bool isManual = false;

        // Check if user manually scrolled mouse wheel
        if (g_manualWheelTriggered.exchange(false)) {
            shouldScroll = true;
            isManual = true;
            PumpMessagesFor(hudWnd, kLongScrollSettleMs);
        } else if (state.autoScroll) {
            DWORD now = GetTickCount();
            if (now - lastAutoScrollTick >= static_cast<DWORD>(kLongScrollWaitMs)) {
                lastAutoScrollTick = now;
                if (!SendLongScrollStep(&state)) {
                    break;
                }
                shouldScroll = true;
                PumpMessagesFor(hudWnd, kLongScrollSettleMs);
            }
        }

        PumpMessagesFor(hudWnd, 20);
        if (state.done || state.cancelled || !IsWindow(hudWnd)) break;

        if (shouldScroll) {
            bool added = SampleStableLongSelection(&state, hudWnd);
            if (added) {
                state.unchangedSteps = 0;
                InvalidateRect(hudWnd, NULL, FALSE);
                if (previewWnd && IsWindow(previewWnd)) {
                    InvalidateRect(previewWnd, NULL, FALSE);
                }
            } else if (!state.lastSampleUnchanged) {
                state.unchangedSteps = 0;
            } else if (state.autoScroll && !isManual) {
                ++state.unchangedSteps;
                if (state.unchangedSteps >= 3) {
                    // Try one more settle step to confirm bottom
                    PumpMessagesFor(hudWnd, kLongScrollWaitMs * 2);
                    if (state.done || state.cancelled || !IsWindow(hudWnd)) break;
                    SendLongScrollStep(&state);
                    PumpMessagesFor(hudWnd, kLongScrollWaitMs);
                    if (SampleStableLongSelection(&state, hudWnd)) {
                        state.unchangedSteps = 0;
                        InvalidateRect(hudWnd, NULL, FALSE);
                        continue;
                    }
                    // Bottom reached!
                    state.captureLimitReached = true;
                    PumpMessagesFor(hudWnd, 200);
                    break;
                }
            }
        }
    }

    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = NULL;
    }

    if (previewWnd && IsWindow(previewWnd)) DestroyWindow(previewWnd);
    if (IsWindow(hudWnd)) DestroyWindow(hudWnd);

    HBITMAP stitched = NULL;
    if (!state.cancelled) {
        stitched = StitchLongFrames(state.frames, state.w, outH);
    }
    for (LongFrame &frame : state.frames) DeleteObject(frame.bitmap);
    return stitched;
}

} // namespace ModernScreenshot
