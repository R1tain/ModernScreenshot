#include "overlay.h"

namespace ModernScreenshot {

static void DrawOverlayModeButton(HDC dc, const RECT &rect, const wchar_t *text, bool active, int offsetX, int offsetY) {
    RECT drawRect = {rect.left - offsetX, rect.top - offsetY, rect.right - offsetX, rect.bottom - offsetY};
    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
        GraphicsPath path;
        BuildRoundedRectPath(&path, static_cast<float>(drawRect.left), static_cast<float>(drawRect.top),
                             static_cast<float>(RectWidth(drawRect)), static_cast<float>(RectHeight(drawRect)), 8.0f);
        SolidBrush fill(active ? Color(255, 99, 102, 241) : Color(200, 20, 24, 32));
        Pen outline(active ? Color(255, 129, 140, 248) : Color(100, 48, 56, 72), active ? 1.2f : 1.0f);
        graphics.FillPath(&fill, &path);
        graphics.DrawPath(&outline, &path);

        Font font(L"Segoe UI", 12.0f, active ? FontStyleBold : FontStyleRegular, UnitPixel);
        SolidBrush textBrush(active ? Color(255, 255, 255, 255) : Color(255, 148, 163, 184));
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        RectF target(static_cast<float>(drawRect.left), static_cast<float>(drawRect.top),
                     static_cast<float>(RectWidth(drawRect)), static_cast<float>(RectHeight(drawRect)));
        graphics.DrawString(text, -1, &font, target, &fmt, &textBrush);
    } else {
        FillRectColor(dc, drawRect, active ? RGB(99, 102, 241) : RGB(20, 24, 32));
        DrawCenteredText(dc, text, drawRect, -13, FW_SEMIBOLD, active ? RGB(255, 255, 255) : RGB(148, 163, 184));
    }
}

static void DrawOverlayOffset(OverlayState *state, HWND hwnd, HDC dc, int offsetX, int offsetY) {
    HDC source = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(source, state->capture.bitmap);
    BitBlt(dc, -offsetX, -offsetY, state->capture.w, state->capture.h, source, 0, 0, SRCCOPY);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        // Cinematic Raycast deep obsidian backdrop scrim
        SolidBrush shade(Color(155, 10, 12, 16));
        graphics.FillRectangle(&shade, 0, 0, state->capture.w, state->capture.h);
    }

    if (state->showModeButtons) {
        int cw = state->capture.w;
        int ch = state->capture.h;
        RECT hintBg = OverlayHintRect(cw, ch);
        if (StartGdiplus()) {
            Graphics graphics(dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
            GraphicsPath hintPath;
            BuildRoundedRectPath(&hintPath, static_cast<float>(hintBg.left - offsetX),
                                 static_cast<float>(hintBg.top - offsetY),
                                 static_cast<float>(RectWidth(hintBg)),
                                 static_cast<float>(RectHeight(hintBg)), 8.0f);
            SolidBrush pillFill(Color(235, 13, 15, 20));
            Pen pillBorder(Color(120, 48, 56, 72), 1.0f);
            graphics.FillPath(&pillFill, &hintPath);
            graphics.DrawPath(&pillBorder, &hintPath);
            const wchar_t *hint = state->kind == CaptureLongRegion
                                      ? L"Drag the scrollable area, then release to start auto capture"
                                      : L"Drag any region. Press Tab to switch to long capture.";
            Font hintFont(L"Segoe UI", 12.0f, FontStyleRegular, UnitPixel);
            SolidBrush hintBrush(Color(255, 148, 163, 184));
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
                          RGB(13, 15, 20));
        }
        DrawOverlayModeButton(dc, OverlayModeRect(cw, ch, 0), L"Region",
                              state->kind == CaptureRegion, offsetX, offsetY);
        DrawOverlayModeButton(dc, OverlayModeRect(cw, ch, 1), L"Long capture",
                              state->kind == CaptureLongRegion, offsetX, offsetY);
    }

    if (state->dragging) {
        RECT local = NormalizeRectPoints(state->start, state->current);
        local = ClampRect(local, 0, 0, state->capture.w, state->capture.h);
        int rw = RectWidth(local);
        int rh = RectHeight(local);
        if (rw > 0 && rh > 0) {
            BitBlt(dc, local.left - offsetX, local.top - offsetY, rw, rh, source,
                   local.left, local.top, SRCCOPY);
            int drawLeft = local.left - offsetX;
            int drawTop = local.top - offsetY;
            int drawRight = local.right - offsetX;
            int drawBottom = local.bottom - offsetY;

            if (StartGdiplus()) {
                Graphics graphics(dc);
                graphics.SetSmoothingMode(SmoothingModeAntiAlias);
                graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

                // Modern Electric Indigo border
                Pen accent(Color(255, 99, 102, 241), 1.5f);
                Pen innerLine(Color(80, 255, 255, 255), 1.0f);
                graphics.DrawRectangle(&accent, drawLeft, drawTop, rw, rh);
                if (rw > 2 && rh > 2) {
                    graphics.DrawRectangle(&innerLine, drawLeft + 1, drawTop + 1, rw - 2, rh - 2);
                }

                // 8 Modern circular dot handles (4 corners + 4 edge centers)
                struct HandlePoint { float x; float y; };
                HandlePoint handles[8] = {
                    { static_cast<float>(drawLeft), static_cast<float>(drawTop) },
                    { static_cast<float>(drawLeft + rw / 2), static_cast<float>(drawTop) },
                    { static_cast<float>(drawRight), static_cast<float>(drawTop) },
                    { static_cast<float>(drawRight), static_cast<float>(drawTop + rh / 2) },
                    { static_cast<float>(drawRight), static_cast<float>(drawBottom) },
                    { static_cast<float>(drawLeft + rw / 2), static_cast<float>(drawBottom) },
                    { static_cast<float>(drawLeft), static_cast<float>(drawBottom) },
                    { static_cast<float>(drawLeft), static_cast<float>(drawTop + rh / 2) }
                };

                const float dotRadius = 4.0f;
                SolidBrush whiteBrush(Color(255, 255, 255, 255));
                Pen dotPen(Color(255, 99, 102, 241), 1.6f);
                Pen shadowPen(Color(120, 0, 0, 0), 1.0f);

                for (const auto &hp : handles) {
                    graphics.DrawEllipse(&shadowPen, hp.x - dotRadius - 0.5f, hp.y - dotRadius - 0.5f,
                                         dotRadius * 2.0f + 1.0f, dotRadius * 2.0f + 1.0f);
                    graphics.FillEllipse(&whiteBrush, hp.x - dotRadius, hp.y - dotRadius,
                                         dotRadius * 2.0f, dotRadius * 2.0f);
                    graphics.DrawEllipse(&dotPen, hp.x - dotRadius, hp.y - dotRadius,
                                         dotRadius * 2.0f, dotRadius * 2.0f);
                }

                // Sleek Floating Dimension HUD: [ • 1920 × 1080 px ]
                wchar_t label[64];
                wsprintfW(label, L"%d × %d", rw, rh);

                int hudW = 108;
                int hudH = 26;
                int hudX = drawLeft;
                int hudY = drawTop - hudH - 8;
                if (hudY < 10) {
                    hudY = drawTop + 10;
                }
                if (hudX + hudW > state->capture.w - 10) {
                    hudX = state->capture.w - hudW - 10;
                }
                if (hudX < 10) {
                    hudX = 10;
                }

                GraphicsPath hudPath;
                BuildRoundedRectPath(&hudPath, static_cast<float>(hudX), static_cast<float>(hudY),
                                     static_cast<float>(hudW), static_cast<float>(hudH), 13.0f);
                SolidBrush hudBg(Color(235, 13, 15, 20));
                Pen hudBorder(Color(180, 56, 64, 82), 1.0f);
                graphics.FillPath(&hudBg, &hudPath);
                graphics.DrawPath(&hudBorder, &hudPath);

                // Small glowing cyan dot
                SolidBrush dotAccent(Color(255, 56, 189, 248));
                graphics.FillEllipse(&dotAccent, static_cast<float>(hudX + 11), static_cast<float>(hudY + 10), 6.0f, 6.0f);

                // Size text in pure crisp white
                Font hudFont(L"Segoe UI", 12.0f, FontStyleBold, UnitPixel);
                SolidBrush hudText(Color(255, 248, 250, 252));
                graphics.DrawString(label, -1, &hudFont,
                                    PointF(static_cast<float>(hudX + 24), static_cast<float>(hudY + 5)),
                                    &hudText);
            } else {
                HPEN pen = CreatePen(PS_SOLID, 2, RGB(99, 102, 241));
                HGDIOBJ oldPen = SelectObject(dc, pen);
                HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                Rectangle(dc, drawLeft, drawTop, drawRight, drawBottom);
                SelectObject(dc, oldBrush);
                SelectObject(dc, oldPen);
                DeleteObject(pen);
            }
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
    RECT label = {local.left, local.top - 36, local.left + 120, local.top};
    if (label.top < 8) {
        OffsetRect(&label, 0, 48);
    }
    label = ClampRect(label, 0, 0, state->capture.w, state->capture.h);
    UnionRect(&bounds, &bounds, &label);
    return ExpandedRect(bounds, 4, state->capture.w, state->capture.h);
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
