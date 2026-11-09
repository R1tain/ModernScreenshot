#include "overlay.h"

namespace ModernScreenshot {

static void DrawOverlayModeButton(HDC dc, const RECT &rect, const wchar_t *text, bool active, int offsetX, int offsetY) {
    RECT drawRect = {rect.left - offsetX, rect.top - offsetY, rect.right - offsetX, rect.bottom - offsetY};
    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        GraphicsPath path;
        BuildRoundedRectPath(&path, static_cast<float>(drawRect.left), static_cast<float>(drawRect.top),
                             static_cast<float>(RectWidth(drawRect)), static_cast<float>(RectHeight(drawRect)), 10.0f);
        SolidBrush fill(active ? Color(245, 23, 198, 163) : Color(230, 16, 21, 24));
        Pen outline(active ? Color(255, 245, 251, 248) : Color(150, 82, 98, 100), active ? 1.4f : 1.0f);
        graphics.FillPath(&fill, &path);
        graphics.DrawPath(&outline, &path);
    } else {
        FillRectColor(dc, drawRect, active ? RGB(23, 198, 163) : RGB(16, 21, 24));
    }
    DrawCenteredText(dc, text, drawRect, -14, FW_SEMIBOLD, active ? RGB(7, 11, 14) : RGB(245, 251, 248));
}

static void DrawOverlayOffset(OverlayState *state, HWND hwnd, HDC dc, int offsetX, int offsetY) {
    HDC source = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(source, state->capture.bitmap);
    BitBlt(dc, -offsetX, -offsetY, state->capture.w, state->capture.h, source, 0, 0, SRCCOPY);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush shade(Color(138, 7, 11, 14));
        graphics.FillRectangle(&shade, 0, 0, state->capture.w, state->capture.h);
    }

    if (state->showModeButtons) {
        int cw = state->capture.w;
        int ch = state->capture.h;
        RECT hintBg = OverlayHintRect(cw, ch);
        if (StartGdiplus()) {
            Graphics graphics(dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            GraphicsPath hintPath;
            BuildRoundedRectPath(&hintPath, static_cast<float>(hintBg.left - offsetX),
                                 static_cast<float>(hintBg.top - offsetY),
                                 static_cast<float>(RectWidth(hintBg)),
                                 static_cast<float>(RectHeight(hintBg)), 10.0f);
            SolidBrush pillFill(Color(220, 16, 21, 24));
            Pen pillBorder(Color(170, 64, 80, 86), 1.0f);
            graphics.FillPath(&pillFill, &hintPath);
            graphics.DrawPath(&pillBorder, &hintPath);
            const wchar_t *hint = state->kind == CaptureLongRegion
                                      ? L"Drag the scrollable area, then release to start auto capture"
                                      : L"Drag any region. Press Tab to switch to long capture.";
            Font hintFont(L"Segoe UI", 12.0f, FontStyleRegular, UnitPixel);
            SolidBrush hintBrush(Color(255, 230, 240, 240));
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            RectF hintRect(static_cast<float>(hintBg.left - offsetX),
                           static_cast<float>(hintBg.top - offsetY),
                           static_cast<float>(RectWidth(hintBg)),
                           static_cast<float>(RectHeight(hintBg)));
            graphics.DrawString(hint, -1, &hintFont, hintRect, &fmt, &hintBrush);
        } else {
            FillRectColor(dc, {hintBg.left - offsetX, hintBg.top - offsetY,
                               hintBg.right - offsetX, hintBg.bottom - offsetY},
                          RGB(16, 21, 24));
        }
        DrawOverlayModeButton(dc, OverlayModeRect(cw, ch, 0), L"Region",
                              state->kind == CaptureRegion, offsetX, offsetY);
        DrawOverlayModeButton(dc, OverlayModeRect(cw, ch, 1), L"Long capture",
                              state->kind == CaptureLongRegion, offsetX, offsetY);
    }

    if (state->dragging) {
        RECT local = NormalizeRectPoints(state->start, state->current);
        local = ClampRect(local, 0, 0, state->capture.w, state->capture.h);
        if (RectWidth(local) > 0 && RectHeight(local) > 0) {
            BitBlt(dc, local.left - offsetX, local.top - offsetY, RectWidth(local), RectHeight(local), source,
                   local.left, local.top, SRCCOPY);
            if (StartGdiplus()) {
                Graphics graphics(dc);
                graphics.SetSmoothingMode(SmoothingModeAntiAlias);
                Pen accent(Color(255, 23, 198, 163), 2.4f);
                Pen white(Color(220, 245, 251, 248), 1.2f);
                int drawLeft = local.left - offsetX;
                int drawTop = local.top - offsetY;
                graphics.DrawRectangle(&accent, drawLeft, drawTop, RectWidth(local), RectHeight(local));
                int handle = 13;
                SolidBrush handleBrush(Color(255, 245, 251, 248));
                graphics.FillEllipse(&handleBrush, drawLeft - handle / 2, drawTop - handle / 2, handle, handle);
                graphics.FillEllipse(&handleBrush, local.right - offsetX - handle / 2, drawTop - handle / 2, handle, handle);
                graphics.FillEllipse(&handleBrush, drawLeft - handle / 2, local.bottom - offsetY - handle / 2, handle, handle);
                graphics.FillEllipse(&handleBrush, local.right - offsetX - handle / 2,
                                     local.bottom - offsetY - handle / 2, handle, handle);
                graphics.DrawRectangle(&white, drawLeft + 1, drawTop + 1,
                                       std::max(0, RectWidth(local) - 2), std::max(0, RectHeight(local) - 2));
            }

            wchar_t label[64];
            wsprintfW(label, L"%d x %d", RectWidth(local), RectHeight(local));
            RECT pill = {local.left, local.top - 28, local.left + 92, local.top - 4};
            if (pill.top < 8) {
                OffsetRect(&pill, 0, 36);
            }
            if (StartGdiplus()) {
                Graphics graphics(dc);
                graphics.SetSmoothingMode(SmoothingModeAntiAlias);
                GraphicsPath path;
                BuildRoundedRectPath(&path, static_cast<float>(pill.left - offsetX),
                                     static_cast<float>(pill.top - offsetY),
                                     static_cast<float>(RectWidth(pill)),
                                     static_cast<float>(RectHeight(pill)), 7.0f);
                SolidBrush brush(Color(236, 16, 21, 24));
                graphics.FillPath(&brush, &path);
            } else {
                RECT drawPill = {pill.left - offsetX, pill.top - offsetY,
                                 pill.right - offsetX, pill.bottom - offsetY};
                FillRectColor(dc, drawPill, RGB(16, 21, 24));
            }
            DrawTextLabel(dc, label, pill.left - offsetX + 10, pill.top - offsetY + 4, RGB(245, 251, 248));
        }
    }

    SelectObject(source, old);
    DeleteDC(source);
    (void)hwnd;
}

static void DrawOverlay(OverlayState *state, HWND hwnd, HDC dc) {
    DrawOverlayOffset(state, hwnd, dc, 0, 0);
}

static RECT OverlayVisualBounds(OverlayState *state) {
    RECT empty = {0, 0, 0, 0};
    if (!state || !state->dragging) {
        return empty;
    }

    RECT local = NormalizeRectPoints(state->start, state->current);
    local = ClampRect(local, 0, 0, state->capture.w, state->capture.h);
    if (RectWidth(local) <= 0 || RectHeight(local) <= 0) {
        return empty;
    }

    RECT bounds = ExpandedRect(local, 20, state->capture.w, state->capture.h);
    RECT label = {local.left, local.top - 28, local.left + 92, local.top - 4};
    if (label.top < 8) {
        OffsetRect(&label, 0, 36);
    }
    label = ClampRect(label, 0, 0, state->capture.w, state->capture.h);
    UnionRect(&bounds, &bounds, &label);
    return ExpandedRect(bounds, 2, state->capture.w, state->capture.h);
}

static void InvalidateOverlayChange(HWND hwnd, OverlayState *state, const RECT &oldBounds) {
    RECT next = OverlayVisualBounds(state);
    if (IsRectEmptyOrInvalid(oldBounds) && IsRectEmptyOrInvalid(next)) {
        return;
    }

    RECT dirty = {};
    if (IsRectEmptyOrInvalid(oldBounds)) {
        dirty = next;
    } else if (IsRectEmptyOrInvalid(next)) {
        dirty = oldBounds;
    } else {
        UnionRect(&dirty, &oldBounds, &next);
    }
    InvalidateRect(hwnd, &dirty, FALSE);
}

static void PaintOverlayBuffered(OverlayState *state, HWND hwnd, HDC dc, const RECT &paint) {
    int w = RectWidth(paint);
    int h = RectHeight(paint);
    if (!state || w <= 0 || h <= 0) {
        return;
    }

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP buffer = CreateCompatibleBitmap(dc, w, h);
    if (!memory || !buffer) {
        if (buffer) DeleteObject(buffer);
        if (memory) DeleteDC(memory);
        DrawOverlay(state, hwnd, dc);
        return;
    }

    HGDIOBJ old = SelectObject(memory, buffer);
    DrawOverlayOffset(state, hwnd, memory, paint.left, paint.top);
    BitBlt(dc, paint.left, paint.top, w, h, memory, 0, 0, SRCCOPY);
    SelectObject(memory, old);
    DeleteObject(buffer);
    DeleteDC(memory);
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    OverlayState *state = reinterpret_cast<OverlayState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        SetCursor(LoadCursor(NULL, IDC_CROSS));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_CROSS));
        return TRUE;
    case WM_KEYDOWN:
        if (!state) return 0;
        if (wParam == VK_ESCAPE) {
            state->cancelled = true;
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == VK_TAB && state->showModeButtons) {
            state->kind = state->kind == CaptureRegion ? CaptureLongRegion : CaptureRegion;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (state) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (state->showModeButtons) {
                bool hitMode = false;
                CaptureKind mode = OverlayModeAt(state->capture.w, state->capture.h, x, y, &hitMode);
                if (hitMode) {
                    state->kind = mode;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
            }
            SetCapture(hwnd);
            state->dragging = true;
            state->start = {x, y};
            state->current = state->start;
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state && state->dragging) {
            RECT oldBounds = OverlayVisualBounds(state);
            state->current = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            InvalidateOverlayChange(hwnd, state, oldBounds);
        }
        return 0;
    case WM_LBUTTONUP:
        if (state && state->dragging) {
            ReleaseCapture();
            state->current = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            RECT local = NormalizeRectPoints(state->start, state->current);
            local = ClampRect(local, 0, 0, state->capture.w, state->capture.h);
            if (RectWidth(local) >= 4 && RectHeight(local) >= 4) {
                state->selection = {local.left + state->capture.x, local.top + state->capture.y,
                                    local.right + state->capture.x, local.bottom + state->capture.y};
            } else {
                state->cancelled = true;
            }
            state->done = true;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            PaintOverlayBuffered(state, hwnd, dc, ps.rcPaint);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool RunSelectionOverlay(HINSTANCE instance, const ScreenCapture &capture, RECT *selection,
                         CaptureKind *kind, bool showModeButtons, CaptureKind initialKind) {
    OverlayState state;
    state.capture = capture;
    state.showModeButtons = showModeButtons;
    state.kind = initialKind;

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kOverlayClass, L"Modern Screenshot",
                                WS_POPUP, capture.x, capture.y, capture.w, capture.h,
                                NULL, NULL, instance, &state);
    if (!hwnd) {
        return false;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    MSG msg;
    while (!state.done && GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (state.cancelled || RectWidth(state.selection) <= 0 || RectHeight(state.selection) <= 0) {
        return false;
    }
    *selection = state.selection;
    if (kind) {
        *kind = state.kind;
    }
    return true;
}

} // namespace ModernScreenshot
