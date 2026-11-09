#include "overlay.h"

namespace ModernScreenshot {

static void DrawOverlayModeButton(HDC dc, const RECT &rect, const wchar_t *text,
                                  bool active, bool hovered, int offsetX, int offsetY) {
    RECT drawRect = {rect.left - offsetX, rect.top - offsetY, rect.right - offsetX, rect.bottom - offsetY};
    DrawGlassButton(dc, drawRect, 10.0f, active, active, hovered, false, false);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        float fx = static_cast<float>(drawRect.left);
        float fy = static_cast<float>(drawRect.top);
        float fw = static_cast<float>(RectWidth(drawRect));
        float fh = static_cast<float>(RectHeight(drawRect));

        // Status indicator dot on the left side of the mode pill
        float dotR = 3.5f;
        float dotCx = fx + 15.0f;
        float dotCy = fy + fh * 0.5f;
        if (active) {
            SolidBrush glow(Color(110, 186, 230, 255));
            SolidBrush dot(Color(255, 255, 255, 255));
            graphics.FillEllipse(&glow, dotCx - dotR - 2.0f, dotCy - dotR - 2.0f, (dotR + 2.0f) * 2.0f, (dotR + 2.0f) * 2.0f);
            graphics.FillEllipse(&dot, dotCx - dotR, dotCy - dotR, dotR * 2.0f, dotR * 2.0f);
        } else {
            Pen ring(hovered ? Color(220, 125, 211, 252) : Color(140, 148, 163, 184), 1.4f);
            graphics.DrawEllipse(&ring, dotCx - dotR, dotCy - dotR, dotR * 2.0f, dotR * 2.0f);
        }

        Font font(L"Segoe UI", 12.5f, (active || hovered) ? FontStyleBold : FontStyleRegular, UnitPixel);
        Color textCol = active ? Color(255, 255, 255, 255)
                               : (hovered ? Color(255, 241, 245, 249) : Color(230, 156, 172, 194));
        SolidBrush textBrush(textCol);
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        RectF target(fx + 10.0f, fy, fw - 14.0f, fh);
        graphics.DrawString(text, -1, &font, target, &fmt, &textBrush);
    } else {
        DrawCenteredText(dc, text, drawRect, -13, FW_SEMIBOLD,
                         active ? RGB(255, 255, 255) : (hovered ? RGB(241, 245, 249) : RGB(156, 172, 194)));
    }
}

static void DrawHighVisibilityCursorReticle(Graphics *graphics, int mx, int my) {
    float cx = static_cast<float>(mx);
    float cy = static_cast<float>(my);

    // Outer dark contrast halo so reticle stands out on pure white screens
    Pen darkHalo(Color(190, 0, 0, 0), 3.2f);
    graphics->DrawEllipse(&darkHalo, cx - 10.0f, cy - 10.0f, 20.0f, 20.0f);
    graphics->DrawLine(&darkHalo, cx - 18.0f, cy, cx - 6.0f, cy);
    graphics->DrawLine(&darkHalo, cx + 6.0f, cy, cx + 18.0f, cy);
    graphics->DrawLine(&darkHalo, cx, cy - 18.0f, cx, cy - 6.0f);
    graphics->DrawLine(&darkHalo, cx, cy + 6.0f, cx, cy + 18.0f);

    // Luminous Electric Sky-Blue & White inner reticle so it stands out on pure black screens
    Pen ringPen(Color(245, 56, 189, 248), 1.6f);
    Pen crossPen(Color(255, 255, 255, 255), 1.6f);
    graphics->DrawEllipse(&ringPen, cx - 10.0f, cy - 10.0f, 20.0f, 20.0f);
    graphics->DrawLine(&crossPen, cx - 17.0f, cy, cx - 6.0f, cy);
    graphics->DrawLine(&crossPen, cx + 6.0f, cy, cx + 17.0f, cy);
    graphics->DrawLine(&crossPen, cx, cy - 17.0f, cx, cy - 6.0f);
    graphics->DrawLine(&crossPen, cx, cy + 6.0f, cx, cy + 17.0f);

    SolidBrush centerDot(Color(255, 56, 189, 248));
    graphics->FillEllipse(&centerDot, cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
}

static void DrawOverlayOffset(OverlayState *state, HWND hwnd, HDC dc, int offsetX, int offsetY) {
    HDC source = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(source, state->capture.bitmap);
    BitBlt(dc, -offsetX, -offsetY, state->capture.w, state->capture.h, source, 0, 0, SRCCOPY);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        // Subtle Apple macOS capture scrim
        SolidBrush shade(Color(122, 10, 14, 22));
        graphics.FillRectangle(&shade, 0, 0, state->capture.w, state->capture.h);

        // Full-screen subtle glass viewport frame
        Pen screenOuter(Color(185, 56, 138, 248), 2.0f);
        Pen screenInner(Color(85, 255, 255, 255), 1.0f);
        graphics.DrawRectangle(&screenOuter, -offsetX + 1, -offsetY + 1, state->capture.w - 2, state->capture.h - 2);
        graphics.DrawRectangle(&screenInner, -offsetX + 3, -offsetY + 3, state->capture.w - 6, state->capture.h - 6);
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

                // Consistent Apple Frosted-Glass Multi-Layer Selection Border
                Pen outerGlow(Color(95, 24, 118, 252), 4.0f);
                Pen accentBorder(Color(255, 56, 152, 255), 2.0f);
                Pen innerSpecular(Color(175, 255, 255, 255), 1.0f);
                graphics.DrawRectangle(&outerGlow, drawLeft - 1, drawTop - 1, rw + 2, rh + 2);
                graphics.DrawRectangle(&accentBorder, drawLeft, drawTop, rw, rh);
                if (rw > 2 && rh > 2) {
                    graphics.DrawRectangle(&innerSpecular, drawLeft + 1, drawTop + 1, rw - 2, rh - 2);
                }

                // 8 circular frosted glass handles
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

                const float dotRadius = 4.4f;
                SolidBrush whiteBrush(Color(255, 255, 255, 255));
                Pen dotPen(Color(255, 24, 118, 252), 1.8f);
                Pen shadowPen(Color(130, 0, 0, 0), 1.4f);

                for (const auto &hp : handles) {
                    graphics.DrawEllipse(&shadowPen, hp.x - dotRadius - 0.5f, hp.y - dotRadius - 0.5f,
                                         dotRadius * 2.0f + 1.0f, dotRadius * 2.0f + 1.0f);
                    graphics.FillEllipse(&whiteBrush, hp.x - dotRadius, hp.y - dotRadius,
                                         dotRadius * 2.0f, dotRadius * 2.0f);
                    graphics.DrawEllipse(&dotPen, hp.x - dotRadius, hp.y - dotRadius,
                                         dotRadius * 2.0f, dotRadius * 2.0f);
                }

                // Apple Frosted Glass Floating Dimension HUD
                wchar_t label[64];
                wsprintfW(label, L"%d × %d", rw, rh);

                int hudW = 114;
                int hudH = 28;
                int hudX = drawLeft;
                int hudY = drawTop - hudH - 10;
                if (hudY < 10) {
                    hudY = drawTop + 10;
                }
                if (hudX + hudW > state->capture.w - 10) {
                    hudX = state->capture.w - hudW - 10;
                }
                if (hudX < 10) {
                    hudX = 10;
                }

                RECT hudRect = {hudX, hudY, hudX + hudW, hudY + hudH};
                DrawGlassPanel(dc, hudRect, 14.0f, true, true);

                SolidBrush dotGlow(Color(95, 56, 189, 248));
                SolidBrush dotAccent(Color(255, 56, 189, 248));
                graphics.FillEllipse(&dotGlow, static_cast<float>(hudX + 9), static_cast<float>(hudY + 9), 10.0f, 10.0f);
                graphics.FillEllipse(&dotAccent, static_cast<float>(hudX + 11), static_cast<float>(hudY + 11), 6.0f, 6.0f);

                Font hudFont(L"Segoe UI", 12.0f, FontStyleBold, UnitPixel);
                SolidBrush hudText(Color(255, 255, 255, 255));
                graphics.DrawString(label, -1, &hudFont,
                                    PointF(static_cast<float>(hudX + 24), static_cast<float>(hudY + 6)),
                                    &hudText);
            } else {
                HPEN pen = CreatePen(PS_SOLID, 2, RGB(56, 152, 255));
                HGDIOBJ oldPen = SelectObject(dc, pen);
                HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                Rectangle(dc, drawLeft, drawTop, drawRight, drawBottom);
                SelectObject(dc, oldBrush);
                SelectObject(dc, oldPen);
                DeleteObject(pen);
            }
        }
    }

    if (state->showModeButtons) {
        int cw = state->capture.w;
        int ch = state->capture.h;
        RECT hintBg = OverlayHintRect(cw, ch);
        RECT hintDraw = {hintBg.left - offsetX, hintBg.top - offsetY,
                         hintBg.right - offsetX, hintBg.bottom - offsetY};
        DrawGlassPanel(dc, hintDraw, 18.0f, true, false);

        if (StartGdiplus()) {
            Graphics graphics(dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
            const wchar_t *hint = state->kind == CaptureLongRegion
                                      ? L"Long Capture: Drag scrollable area, then release"
                                      : L"Region Capture: Drag area  •  Click below or Tab to switch";
            Font hintFont(L"Segoe UI", 12.0f, FontStyleRegular, UnitPixel);
            SolidBrush hintBrush(Color(245, 241, 245, 249));
            Gdiplus::StringFormat fmt;
            fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
            fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            RectF hintRect(static_cast<float>(hintDraw.left),
                           static_cast<float>(hintDraw.top),
                           static_cast<float>(RectWidth(hintDraw)),
                           static_cast<float>(RectHeight(hintDraw)));
            graphics.DrawString(hint, -1, &hintFont, hintRect, &fmt, &hintBrush);
        }

        // Apple macOS Frosted Glass Segmented Mode Dock
        RECT m0 = OverlayModeRect(cw, ch, 0);
        RECT m1 = OverlayModeRect(cw, ch, 1);
        RECT dockRect = {m0.left - 8 - offsetX, m0.top - 6 - offsetY,
                         m1.right + 8 - offsetX, m1.bottom + 6 - offsetY};
        DrawGlassPanel(dc, dockRect, 14.0f, true, state->hoverMode != -1);

        DrawOverlayModeButton(dc, m0, L"Region",
                              state->kind == CaptureRegion, state->hoverMode == 0, offsetX, offsetY);
        DrawOverlayModeButton(dc, m1, L"Long capture",
                              state->kind == CaptureLongRegion, state->hoverMode == 1, offsetX, offsetY);
    }

    // Draw high-contrast custom crosshair reticle when selecting (and not hovering mode buttons)
    if (state->hasMouse && state->hoverMode == -1 && StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        DrawHighVisibilityCursorReticle(&graphics, state->mouse.x - offsetX, state->mouse.y - offsetY);
    }

    SelectObject(source, old);
    DeleteDC(source);
    (void)hwnd;
}

static void DrawOverlay(OverlayState *state, HWND hwnd, HDC dc) {
    DrawOverlayOffset(state, hwnd, dc, 0, 0);
}

static RECT OverlayModeDockBounds(const OverlayState *state) {
    int cw = state->capture.w;
    int ch = state->capture.h;
    RECT m0 = OverlayModeRect(cw, ch, 0);
    RECT m1 = OverlayModeRect(cw, ch, 1);
    RECT dock = {m0.left - 12, m0.top - 10, m1.right + 12, m1.bottom + 10};
    return ClampRect(dock, 0, 0, cw, ch);
}

static RECT OverlayCursorBounds(POINT pt, int maxW, int maxH) {
    RECT r = {pt.x - 24, pt.y - 24, pt.x + 24, pt.y + 24};
    return ClampRect(r, 0, 0, maxW, maxH);
}

static RECT OverlayVisualBounds(OverlayState *state) {
    RECT empty = {0, 0, 0, 0};
    if (!state) return empty;

    RECT bounds = empty;
    if (state->dragging) {
        RECT local = NormalizeRectPoints(state->start, state->current);
        local = ClampRect(local, 0, 0, state->capture.w, state->capture.h);
        if (RectWidth(local) > 0 && RectHeight(local) > 0) {
            bounds = ExpandedRect(local, 28, state->capture.w, state->capture.h);
            RECT label = {local.left, local.top - 44, local.left + 132, local.top};
            if (label.top < 8) {
                OffsetRect(&label, 0, 56);
            }
            label = ClampRect(label, 0, 0, state->capture.w, state->capture.h);
            UnionRect(&bounds, &bounds, &label);
            bounds = ExpandedRect(bounds, 4, state->capture.w, state->capture.h);
        }
    }

    if (state->hasMouse) {
        RECT cur = OverlayCursorBounds(state->mouse, state->capture.w, state->capture.h);
        if (IsRectEmptyOrInvalid(bounds)) {
            bounds = cur;
        } else {
            UnionRect(&bounds, &bounds, &cur);
        }
    }
    return bounds;
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

static void UpdateOverlayCursor(OverlayState *state) {
    if (state && state->showModeButtons && !state->dragging && state->hoverMode != -1) {
        SetCursor(LoadCursor(NULL, IDC_HAND));
    } else {
        SetCursor(LoadCursor(NULL, IDC_CROSS));
    }
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
        UpdateOverlayCursor(state);
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
            state->hoverMode = -1;
            UpdateOverlayCursor(state);
            SetCapture(hwnd);
            state->dragging = true;
            state->start = {x, y};
            state->current = state->start;
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state) {
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            RECT oldBounds = OverlayVisualBounds(state);
            state->mouse = {mx, my};
            state->hasMouse = true;

            if (state->dragging) {
                state->current = {mx, my};
                InvalidateOverlayChange(hwnd, state, oldBounds);
            } else {
                int newHover = -1;
                if (state->showModeButtons) {
                    bool hitMode = false;
                    CaptureKind mode = OverlayModeAt(state->capture.w, state->capture.h, mx, my, &hitMode);
                    if (hitMode) {
                        newHover = (mode == CaptureRegion) ? 0 : 1;
                    }
                }
                if (newHover != state->hoverMode) {
                    state->hoverMode = newHover;
                    RECT dock = OverlayModeDockBounds(state);
                    InvalidateRect(hwnd, &dock, FALSE);
                }
                UpdateOverlayCursor(state);
                InvalidateOverlayChange(hwnd, state, oldBounds);
            }
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

    POINT initPt = {};
    if (GetCursorPos(&initPt)) {
        state.mouse = {initPt.x - capture.x, initPt.y - capture.y};
        state.hasMouse = true;
        if (showModeButtons) {
            bool hitMode = false;
            CaptureKind mode = OverlayModeAt(capture.w, capture.h, state.mouse.x, state.mouse.y, &hitMode);
            if (hitMode) {
                state.hoverMode = (mode == CaptureRegion) ? 0 : 1;
            }
        }
    }

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
