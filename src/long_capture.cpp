#include "long_capture.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace ModernScreenshot {

static bool IsOwnWindow(HWND hwnd) {
    while (hwnd) {
        wchar_t className[96] = {};
        GetClassNameW(hwnd, className, 96);
        if (wcscmp(className, kHiddenClass) == 0 || wcscmp(className, kOverlayClass) == 0 ||
            wcscmp(className, kLongSessionClass) == 0 || wcscmp(className, kEditorClass) == 0 ||
            wcscmp(className, kSettingsClass) == 0) {
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

static void ClearPendingLongFrame(LongSessionState *state) {
    if (!state) return;
    if (state->pendingBitmap) {
        DeleteObject(state->pendingBitmap);
        state->pendingBitmap = NULL;
    }
    state->pendingPixels.clear();
    state->pendingStableFrames = 0;
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

static int OverlapError(const std::vector<BYTE> &prev, const std::vector<BYTE> &next, int w, int h, int overlap) {
    // Exclude rightmost margin to ignore scrolling scrollbars
    int marginR = std::min(18, w / 10);
    int activeW = std::max(1, w - marginR);

    int xStep = std::max(1, activeW / 56);
    int yStep = std::max(1, overlap / 56);
    long long diff = 0;
    int samples = 0;
    for (int y = 0; y < overlap; y += yStep) {
        int prevY = h - overlap + y;
        for (int x = 0; x < activeW; x += xStep) {
            diff += PixelAbsDiff(PixelAt(prev, w, x, prevY), PixelAt(next, w, x, y));
            ++samples;
        }
    }
    return samples > 0 ? static_cast<int>(diff / samples) : 255;
}

static int EstimateOverlap(const std::vector<BYTE> &prev, const std::vector<BYTE> &next, int w, int h, int *outError) {
    if (w <= 0 || h <= 1) {
        if (outError) *outError = 255 * 3;
        return 0;
    }

    int minOverlap = std::min(h - 1, std::max(48, h / 6));
    int maxOverlap = h - 1;
    int step = std::max(1, h / 120);
    int bestOverlap = minOverlap;
    int bestError = 255 * 3;

    // Coarse pass across valid overlap range
    for (int overlap = minOverlap; overlap <= maxOverlap; overlap += step) {
        int error = OverlapError(prev, next, w, h, overlap);
        if (error < bestError) {
            bestError = error;
            bestOverlap = overlap;
        }
    }

    // Refinement pass around best candidate
    int refineStart = std::max(minOverlap, bestOverlap - step);
    int refineEnd = std::min(maxOverlap, bestOverlap + step);
    for (int overlap = refineStart; overlap <= refineEnd; ++overlap) {
        int error = OverlapError(prev, next, w, h, overlap);
        if (error < bestError) {
            bestError = error;
            bestOverlap = overlap;
        }
    }

    // Tail pass near bottom
    int tailStart = std::max(minOverlap, h - std::max(96, h / 6));
    for (int overlap = tailStart; overlap <= maxOverlap; ++overlap) {
        int error = OverlapError(prev, next, w, h, overlap);
        if (error < bestError) {
            bestError = error;
            bestOverlap = overlap;
        }
    }

    if (outError) *outError = bestError;
    return bestError <= kLongOverlapErrorLimit ? bestOverlap : 0;
}

static bool AppendPendingLongTail(LongSessionState *state) {
    if (!state || !state->pendingBitmap || state->pendingPixels.empty()) {
        return false;
    }
    int overlapError = 255 * 3;
    int overlap = EstimateOverlap(state->prevPixels, state->pendingPixels, state->w, state->h, &overlapError);
    if (overlap <= 0 || overlap >= state->h) {
        return false;
    }

    HBITMAP bitmap = state->pendingBitmap;
    state->pendingBitmap = NULL;
    bool appended = AppendLongFrameStrip(state, bitmap, overlap);
    DeleteObject(bitmap);
    if (!appended) {
        state->pendingPixels.clear();
        state->pendingStableFrames = 0;
        return false;
    }
    state->prevPixels.swap(state->pendingPixels);
    state->pendingPixels.clear();
    state->pendingStableFrames = 0;
    return true;
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
        ClearPendingLongFrame(state);
        return true;
    }

    if (IsSameFramePixels(state->prevPixels, *pixels)) {
        state->lastSampleUnchanged = true;
        DeleteObject(bitmap);
        return false;
    }

    int overlapError = 255 * 3;
    int overlap = EstimateOverlap(state->prevPixels, *pixels, state->w, state->h, &overlapError);
    if (overlap <= 0) {
        if (AverageFrameDiff(state->prevPixels, *pixels, state->w, state->h) <= 1) {
            state->lastSampleUnchanged = true;
        } else if (state->pendingBitmap && AverageFrameDiff(state->pendingPixels, *pixels, state->w, state->h) <= 2) {
            ++state->pendingStableFrames;
            DeleteObject(state->pendingBitmap);
            state->pendingBitmap = bitmap;
            state->pendingPixels.swap(*pixels);
            return false;
        } else {
            ClearPendingLongFrame(state);
            state->pendingBitmap = bitmap;
            state->pendingPixels.swap(*pixels);
            state->pendingStableFrames = 1;
            ++state->skippedFrames;
            return false;
        }
        ++state->skippedFrames;
        DeleteObject(bitmap);
        return false;
    }

    bool appended = AppendLongFrameStrip(state, bitmap, overlap);
    DeleteObject(bitmap);
    if (!appended) return false;
    state->prevPixels.swap(*pixels);
    ClearPendingLongFrame(state);
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

static bool CandidateLongPanelPosition(const RECT &selection, int x, int y, int *outX, int *outY) {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    x = Clamp(x, vx + 8, vx + vw - kLongSessionW - 8);
    y = Clamp(y, vy + 8, vy + vh - kLongSessionH - 8);

    RECT panel = {x, y, x + kLongSessionW, y + kLongSessionH};
    if (RectIntersects(panel, selection)) return false;
    *outX = x;
    *outY = y;
    return true;
}

static void LongPanelPosition(const RECT &selection, int *outX, int *outY) {
    const int gap = 16;
    int candidates[][2] = {
        {selection.right + gap, selection.top},
        {selection.left - kLongSessionW - gap, selection.top},
        {selection.right + gap, selection.bottom - kLongSessionH},
        {selection.left - kLongSessionW - gap, selection.bottom - kLongSessionH},
        {selection.left, selection.bottom + gap},
        {selection.left, selection.top - kLongSessionH - gap},
    };

    for (const auto &candidate : candidates) {
        if (CandidateLongPanelPosition(selection, candidate[0], candidate[1], outX, outY)) {
            return;
        }
    }

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    *outX = Clamp(selection.right + gap, vx + 8, vx + vw - kLongSessionW - 8);
    *outY = Clamp(selection.top, vy + 8, vy + vh - kLongSessionH - 8);
}

static RECT LongDoneRect() {
    return {176, 470, 256, 504};
}

static RECT LongCancelRect() {
    return {266, 470, 346, 504};
}

static void DrawLongPreview(LongSessionState *state, HDC dc, const RECT &preview) {
    FillRectColor(dc, preview, RGB(8, 10, 14));
    if (!state || state->frames.empty() || state->w <= 0 || state->totalH <= 0) {
        DrawCenteredText(dc, L"Preview will appear after first frame", preview, -12, FW_MEDIUM, RGB(100, 116, 139));
        return;
    }

    int padding = 8;
    RECT content = {preview.left + padding, preview.top + padding, preview.right - padding, preview.bottom - padding};
    int contentW = RectWidth(content);
    int contentH = RectHeight(content);
    if (contentW <= 0 || contentH <= 0) return;

    int drawW = contentW;
    int totalScaledH = std::max(1, state->totalH * drawW / state->w);
    int visibleTop = std::max(0, totalScaledH - contentH);
    int drawX = content.left;

    HDC source = CreateCompatibleDC(dc);
    if (!source) return;

    int scaledY = 0;
    for (const LongFrame &frame : state->frames) {
        int frameScaledH = std::max(1, frame.copyH * drawW / state->w);
        int destTop = content.top + scaledY - visibleTop;
        int destBottom = destTop + frameScaledH;
        if (destBottom > content.top && destTop < content.bottom) {
            int clippedTop = std::max(destTop, static_cast<int>(content.top));
            int clippedBottom = std::min(destBottom, static_cast<int>(content.bottom));
            int clippedH = clippedBottom - clippedTop;
            int srcY = std::max(0, (clippedTop - destTop) * frame.copyH / frameScaledH);
            int srcH = std::max(1, clippedH * frame.copyH / frameScaledH);
            HGDIOBJ oldSource = SelectObject(source, frame.bitmap);
            SetStretchBltMode(dc, HALFTONE);
            StretchBlt(dc, drawX, clippedTop, drawW, clippedH,
                       source, 0, srcY, state->w, srcH, SRCCOPY);
            SelectObject(source, oldSource);
        }
        scaledY += frameScaledH;
    }

    DeleteDC(source);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        Pen border(Color(180, 40, 46, 60), 1.0f);
        graphics.DrawRectangle(&border, preview.left, preview.top, RectWidth(preview) - 1, RectHeight(preview) - 1);
        SolidBrush fade(Color(220, 13, 15, 20));
        graphics.FillRectangle(&fade, preview.left + 1, preview.top + 1, RectWidth(preview) - 2, 26);
    }

    wchar_t label[96];
    wsprintfW(label, L"Live preview  •  %d × %d", state->w, state->totalH);
    DrawTextLabel(dc, label, preview.left + 12, preview.top + 7, RGB(248, 250, 252));
}

static void DrawLongSession(HWND hwnd, LongSessionState *state, HDC dc) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    FillRectColor(dc, rect, RGB(13, 15, 18));

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush panel(Color(250, 13, 15, 18));
        graphics.FillRectangle(&panel, 0, 0, RectWidth(rect), RectHeight(rect));
        Pen border(Color(220, 99, 102, 241), 1.2f);
        graphics.DrawRectangle(&border, 0, 0, RectWidth(rect) - 1, RectHeight(rect) - 1);
    }

    wchar_t status[128];
    wsprintfW(status, L"Long capture  •  %d frames  •  %d px", static_cast<int>(state->frames.size()), state->totalH);
    DrawTextLabel(dc, status, 14, 12, RGB(248, 250, 252));

    if (!state->started) {
        DrawTextLabel(dc, L"Preparing automatic scroll capture...", 14, 34, RGB(148, 163, 184));
    } else if (state->captureLimitReached) {
        DrawTextLabel(dc, L"Finished or limit reached. Opening editor.", 14, 34, RGB(245, 158, 11));
    } else if (state->skippedFrames > 0) {
        DrawTextLabel(dc, L"Waiting for stable overlap...", 14, 34, RGB(245, 158, 11));
    } else if (!state->statusText.empty()) {
        DrawTextLabel(dc, state->statusText, 14, 34, RGB(148, 163, 184));
    } else {
        wchar_t hint[128];
        wsprintfW(hint, L"Auto scrolling. Last +%d px", state->lastCopyH);
        DrawTextLabel(dc, hint, 14, 34, RGB(148, 163, 184));
    }

    RECT preview = {14, 62, 346, 458};
    DrawLongPreview(state, dc, preview);

    RECT done = LongDoneRect();
    RECT cancel = LongCancelRect();
    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        GraphicsPath donePath;
        BuildRoundedRectPath(&donePath, static_cast<float>(done.left), static_cast<float>(done.top),
                             static_cast<float>(RectWidth(done)), static_cast<float>(RectHeight(done)), 8.0f);
        SolidBrush doneFill(Color(255, 99, 102, 241));
        graphics.FillPath(&doneFill, &donePath);

        GraphicsPath cancelPath;
        BuildRoundedRectPath(&cancelPath, static_cast<float>(cancel.left), static_cast<float>(cancel.top),
                             static_cast<float>(RectWidth(cancel)), static_cast<float>(RectHeight(cancel)), 8.0f);
        SolidBrush cancelFill(Color(255, 30, 35, 48));
        Pen cancelBorder(Color(180, 56, 64, 82), 1.0f);
        graphics.FillPath(&cancelFill, &cancelPath);
        graphics.DrawPath(&cancelBorder, &cancelPath);
    } else {
        FillRectColor(dc, done, RGB(99, 102, 241));
        FillRectColor(dc, cancel, RGB(30, 35, 48));
    }
    DrawCenteredText(dc, L"Stop", done, -14, FW_SEMIBOLD, RGB(255, 255, 255));
    DrawCenteredText(dc, L"Cancel", cancel, -14, FW_SEMIBOLD, RGB(226, 232, 240));
}

static void PaintLongSessionBuffered(HWND hwnd, LongSessionState *state, HDC dc, const RECT &paint) {
    int w = RectWidth(paint);
    int h = RectHeight(paint);
    if (!state || w <= 0 || h <= 0) return;

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP buffer = CreateCompatibleBitmap(dc, w, h);
    if (!memory || !buffer) {
        if (buffer) DeleteObject(buffer);
        if (memory) DeleteDC(memory);
        DrawLongSession(hwnd, state, dc);
        return;
    }

    HGDIOBJ old = SelectObject(memory, buffer);
    SetViewportOrgEx(memory, -paint.left, -paint.top, NULL);
    DrawLongSession(hwnd, state, memory);
    SetViewportOrgEx(memory, 0, 0, NULL);
    BitBlt(dc, paint.left, paint.top, w, h, memory, 0, 0, SRCCOPY);
    SelectObject(memory, old);
    DeleteObject(buffer);
    DeleteDC(memory);
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
    case WM_COMMAND:
        if (LOWORD(wParam) == kLongSessionDone && state) {
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wParam) == kLongSessionCancel && state) {
            state->cancelled = true;
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN && state) {
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == VK_ESCAPE && state) {
            state->cancelled = true;
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (state) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            RECT done = LongDoneRect();
            RECT cancel = LongCancelRect();
            if (PointInRect(x, y, done)) {
                state->done = true;
                DestroyWindow(hwnd);
            } else if (PointInRect(x, y, cancel)) {
                state->cancelled = true;
                state->done = true;
                DestroyWindow(hwnd);
            } else {
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            PaintLongSessionBuffered(hwnd, state, dc, ps.rcPaint);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        if (state) state->done = true;
        DestroyWindow(hwnd);
        return 0;
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

    int x = 0;
    int y = 0;
    LongPanelPosition(selection, &x, &y);

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                kLongSessionClass, L"ModernScreenshot Long Capture",
                                WS_POPUP, x, y, kLongSessionW, kLongSessionH, NULL, NULL, instance, &state);
    if (!hwnd) {
        for (LongFrame &frame : state.frames) DeleteObject(frame.bitmap);
        ClearPendingLongFrame(&state);
        return NULL;
    }

    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);
    FocusLongTarget(&state);
    SetCursorPos(center.x, center.y);

    state.started = true;
    state.statusText = L"Auto scrolling...";
    InvalidateRect(hwnd, NULL, FALSE);
    PumpMessagesFor(hwnd, 120);

    while (!state.done && !state.cancelled && IsWindow(hwnd)) {
        if (state.totalH >= kLongCaptureMaxHeight ||
            static_cast<int>(state.frames.size()) >= kLongCaptureMaxFrames) {
            state.captureLimitReached = true;
            state.statusText = L"Limit reached.";
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }

        if (!SendLongScrollStep(&state)) {
            state.statusText = L"Target did not accept scrolling.";
            InvalidateRect(hwnd, NULL, FALSE);
            PumpMessagesFor(hwnd, 400);
            break;
        }

        PumpMessagesFor(hwnd, kLongScrollWaitMs);
        if (state.done || state.cancelled || !IsWindow(hwnd)) break;

        bool added = SampleStableLongSelection(&state, hwnd);
        if (added) {
            state.unchangedSteps = 0;
            state.statusText = L"Auto scrolling...";
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (!state.lastSampleUnchanged) {
            state.unchangedSteps = 0;
            state.statusText = state.pendingStableFrames >= 2 ? L"At bottom or content changed. Verifying..." :
                                                                 L"Waiting for stable overlap...";
            InvalidateRect(hwnd, NULL, FALSE);
        } else {
            ++state.unchangedSteps;
            state.statusText = L"Checking for page bottom...";
            InvalidateRect(hwnd, NULL, FALSE);
            if (state.unchangedSteps >= 3) {
                PumpMessagesFor(hwnd, kLongScrollWaitMs * 2);
                if (state.done || state.cancelled || !IsWindow(hwnd)) break;
                SendLongScrollStep(&state);
                PumpMessagesFor(hwnd, kLongScrollWaitMs);
                if (state.done || state.cancelled || !IsWindow(hwnd)) break;

                if (SampleStableLongSelection(&state, hwnd)) {
                    state.unchangedSteps = 0;
                    state.statusText = L"Auto scrolling...";
                    InvalidateRect(hwnd, NULL, FALSE);
                    continue;
                }
                if (!state.lastSampleUnchanged) {
                    state.unchangedSteps = 0;
                    state.statusText = L"Waiting for stable overlap...";
                    InvalidateRect(hwnd, NULL, FALSE);
                    continue;
                }
                if (state.pendingStableFrames >= 2 && AppendPendingLongTail(&state)) {
                    state.unchangedSteps = 0;
                    state.statusText = L"Added final tail.";
                    InvalidateRect(hwnd, NULL, FALSE);
                    continue;
                }
                if (state.pendingStableFrames >= 1 && AppendPendingLongTail(&state)) {
                    state.unchangedSteps = 0;
                    state.statusText = L"Added final tail.";
                    InvalidateRect(hwnd, NULL, FALSE);
                    continue;
                }
                state.captureLimitReached = true;
                state.statusText = L"Bottom reached.";
                InvalidateRect(hwnd, NULL, FALSE);
                PumpMessagesFor(hwnd, 350);
                break;
            }
        }
    }

    if (IsWindow(hwnd)) DestroyWindow(hwnd);

    HBITMAP stitched = NULL;
    if (!state.cancelled) {
        stitched = StitchLongFrames(state.frames, state.w, outH);
    }
    for (LongFrame &frame : state.frames) DeleteObject(frame.bitmap);
    ClearPendingLongFrame(&state);
    return stitched;
}

} // namespace ModernScreenshot
