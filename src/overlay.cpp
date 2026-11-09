#include "overlay.h"

namespace ModernScreenshot {

static RECT OverlayHudRect(const RECT &local, int captureW, int captureH) {
    int hudW = 114;
    int hudH = 28;
    int hudX = local.left;
    int hudY = local.top - hudH - 10;
    if (hudY < 10) {
        hudY = local.top + 10;
    }
    if (hudX + hudW > captureW - 10) {
        hudX = captureW - hudW - 10;
    }
    if (hudX < 10) {
        hudX = 10;
    }
    RECT r = {hudX, hudY, hudX + hudW, hudY + hudH};
    return ClampRect(r, 0, 0, captureW, captureH);
}

static RECT OverlayModeDockBounds(const OverlayState *state) {
    int cw = state->capture.w;
    int ch = state->capture.h;
    RECT m0 = OverlayModeRect(cw, ch, 0);
    RECT m1 = OverlayModeRect(cw, ch, 1);
    RECT dock = {m0.left - 12, m0.top - 10, m1.right + 12, m1.bottom + 10};
    return ClampRect(dock, 0, 0, cw, ch);
}

static void DrawOverlayModeButton(HDC dc, const RECT &rect, const wchar_t *text,
                                  bool active, bool hovered) {
    DrawGlassButton(dc, rect, 10.0f, active, active, hovered, false, false);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        float fx = static_cast<float>(rect.left);
        float fy = static_cast<float>(rect.top);
        float fw = static_cast<float>(RectWidth(rect));
        float fh = static_cast<float>(RectHeight(rect));

        // Status indicator dot on the left side of the mode pill
        float dotR = 3.5f;
        float dotCx = fx + 15.0f;
        float dotCy = fy + fh * 0.5f;
        if (active) {
            SolidBrush glow(Color(110, 186, 230, 255));
            SolidBrush dot(Color(255, 255, 255, 255));
            graphics.FillEllipse(&glow, dotCx - dotR - 2.0f, dotCy - dotR - 2.0f,
                                 (dotR + 2.0f) * 2.0f, (dotR + 2.0f) * 2.0f);
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
        DrawCenteredText(dc, text, rect, -13, FW_SEMIBOLD,
                         active ? RGB(255, 255, 255) : (hovered ? RGB(241, 245, 249) : RGB(156, 172, 194)));
    }
}

static void RenderOverlayToFullDc(OverlayState *state, HDC dc, const RECT &dirty) {
    int cw = state->capture.w;
    int ch = state->capture.h;
    RECT clip = ClampRect(dirty, 0, 0, cw, ch);
    int clipW = RectWidth(clip);
    int clipH = RectHeight(clip);
    if (clipW <= 0 || clipH <= 0) return;

    HDC source = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(source, state->capture.bitmap);

    // 1. Restore clean desktop screenshot over the dirty region (plus margin for blur kernels)
    RECT restore = ExpandedRect(clip, 18, cw, ch);
    BitBlt(dc, restore.left, restore.top, RectWidth(restore), RectHeight(restore),
           source, restore.left, restore.top, SRCCOPY);

    int savedClip = SaveDC(dc);
    IntersectClipRect(dc, clip.left, clip.top, clip.right, clip.bottom);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        // Subtle Apple macOS capture scrim over the dirty region
        SolidBrush shade(Color(122, 10, 14, 22));
        graphics.FillRectangle(&shade, clip.left, clip.top, clipW, clipH);

        // Full-screen subtle glass viewport frame
        Pen screenOuter(Color(185, 56, 138, 248), 2.0f);
        Pen screenInner(Color(85, 255, 255, 255), 1.0f);
        graphics.DrawRectangle(&screenOuter, 1, 1, cw - 2, ch - 2);
        graphics.DrawRectangle(&screenInner, 3, 3, cw - 6, ch - 6);
    }

    // 2. Draw active selection region and floating HUD in unified screen-local coordinates
    if (state->dragging) {
        RECT local = NormalizeRectPoints(state->start, state->current);
        local = ClampRect(local, 0, 0, cw, ch);
        int rw = RectWidth(local);
        int rh = RectHeight(local);
        if (rw > 0 && rh > 0) {
            BitBlt(dc, local.left, local.top, rw, rh, source, local.left, local.top, SRCCOPY);

            if (StartGdiplus()) {
                Graphics graphics(dc);
                graphics.SetSmoothingMode(SmoothingModeAntiAlias);
                graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

                // Consistent Apple Frosted-Glass Multi-Layer Selection Border
                Pen outerGlow(Color(95, 24, 118, 252), 4.0f);
                Pen accentBorder(Color(255, 56, 152, 255), 2.0f);
                Pen innerSpecular(Color(175, 255, 255, 255), 1.0f);
                graphics.DrawRectangle(&outerGlow, local.left - 1, local.top - 1, rw + 2, rh + 2);
                graphics.DrawRectangle(&accentBorder, local.left, local.top, rw, rh);
                if (rw > 2 && rh > 2) {
                    graphics.DrawRectangle(&innerSpecular, local.left + 1, local.top + 1, rw - 2, rh - 2);
                }

                // 8 circular frosted glass handles
                struct HandlePoint { float x; float y; };
                HandlePoint handles[8] = {
                    { static_cast<float>(local.left), static_cast<float>(local.top) },
                    { static_cast<float>(local.left + rw / 2), static_cast<float>(local.top) },
                    { static_cast<float>(local.right), static_cast<float>(local.top) },
                    { static_cast<float>(local.right), static_cast<float>(local.top + rh / 2) },
                    { static_cast<float>(local.right), static_cast<float>(local.bottom) },
                    { static_cast<float>(local.left + rw / 2), static_cast<float>(local.bottom) },
                    { static_cast<float>(local.left), static_cast<float>(local.bottom) },
                    { static_cast<float>(local.left), static_cast<float>(local.top + rh / 2) }
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
                RECT hudRect = OverlayHudRect(local, cw, ch);
                if (RectIntersects(clip, ExpandedRect(hudRect, 6, cw, ch))) {
                    DrawGlassPanel(dc, hudRect, 14.0f, true, true);

                    wchar_t label[64];
                    wsprintfW(label, L"%d × %d", rw, rh);

                    SolidBrush dotGlow(Color(95, 56, 189, 248));
                    SolidBrush dotAccent(Color(255, 56, 189, 248));
                    graphics.FillEllipse(&dotGlow, static_cast<float>(hudRect.left + 9),
                                         static_cast<float>(hudRect.top + 9), 10.0f, 10.0f);
                    graphics.FillEllipse(&dotAccent, static_cast<float>(hudRect.left + 11),
                                         static_cast<float>(hudRect.top + 11), 6.0f, 6.0f);

                    Font hudFont(L"Segoe UI", 12.0f, FontStyleBold, UnitPixel);
                    SolidBrush hudText(Color(255, 255, 255, 255));
                    graphics.DrawString(label, -1, &hudFont,
                                        PointF(static_cast<float>(hudRect.left + 24),
                                               static_cast<float>(hudRect.top + 6)),
                                        &hudText);
                }
            } else {
                HPEN pen = CreatePen(PS_SOLID, 2, RGB(56, 152, 255));
                HGDIOBJ oldPen = SelectObject(dc, pen);
                HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                Rectangle(dc, local.left, local.top, local.right, local.bottom);
                SelectObject(dc, oldBrush);
                SelectObject(dc, oldPen);
                DeleteObject(pen);
            }
        }
    }

    // 3. Draw bottom frosted glass hint & mode switcher dock when within dirty clip
    if (state->showModeButtons) {
        RECT hintRect = OverlayHintRect(cw, ch);
        if (RectIntersects(clip, ExpandedRect(hintRect, 8, cw, ch))) {
            DrawGlassPanel(dc, hintRect, 18.0f, true, false);
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
                RectF target(static_cast<float>(hintRect.left),
                             static_cast<float>(hintRect.top),
                             static_cast<float>(RectWidth(hintRect)),
                             static_cast<float>(RectHeight(hintRect)));
                graphics.DrawString(hint, -1, &hintFont, target, &fmt, &hintBrush);
            }
        }

        RECT m0 = OverlayModeRect(cw, ch, 0);
        RECT m1 = OverlayModeRect(cw, ch, 1);
        RECT dockRect = {m0.left - 8, m0.top - 6, m1.right + 8, m1.bottom + 6};
        if (RectIntersects(clip, ExpandedRect(dockRect, 8, cw, ch))) {
            DrawGlassPanel(dc, dockRect, 14.0f, true, state->hoverMode != -1);
            DrawOverlayModeButton(dc, m0, L"Region",
                                  state->kind == CaptureRegion, state->hoverMode == 0);
            DrawOverlayModeButton(dc, m1, L"Long capture",
                                  state->kind == CaptureLongRegion, state->hoverMode == 1);
        }
    }

    RestoreDC(dc, savedClip);
    SelectObject(source, old);
    DeleteDC(source);
}

static RECT OverlayVisualBounds(OverlayState *state) {
    RECT empty = {0, 0, 0, 0};
    if (!state || !state->dragging) return empty;

    RECT local = NormalizeRectPoints(state->start, state->current);
    local = ClampRect(local, 0, 0, state->capture.w, state->capture.h);
    if (RectWidth(local) <= 0 || RectHeight(local) <= 0) {
        return empty;
    }

    RECT bounds = ExpandedRect(local, 16, state->capture.w, state->capture.h);
    RECT hud = OverlayHudRect(local, state->capture.w, state->capture.h);
    UnionRect(&bounds, &bounds, &hud);
    return ExpandedRect(bounds, 8, state->capture.w, state->capture.h);
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

static bool EnsureOverlayBackbuffer(OverlayState *state, HDC refDc) {
    if (!state) return false;
    if (state->backDc && state->backBitmap) return true;

    state->backDc = CreateCompatibleDC(refDc);
    state->backBitmap = CreateCompatibleBitmap(refDc, state->capture.w, state->capture.h);
    if (!state->backDc || !state->backBitmap) {
        if (state->backBitmap) {
            DeleteObject(state->backBitmap);
            state->backBitmap = NULL;
        }
        if (state->backDc) {
            DeleteDC(state->backDc);
            state->backDc = NULL;
        }
        return false;
    }
    state->backOldObj = SelectObject(state->backDc, state->backBitmap);
    RECT full = {0, 0, state->capture.w, state->capture.h};
    RenderOverlayToFullDc(state, state->backDc, full);
    return true;
}

static void DestroyOverlayBackbuffer(OverlayState *state) {
    if (!state) return;
    if (state->backDc && state->backOldObj) {
        SelectObject(state->backDc, state->backOldObj);
        state->backOldObj = NULL;
    }
    if (state->backBitmap) {
        DeleteObject(state->backBitmap);
        state->backBitmap = NULL;
    }
    if (state->backDc) {
        DeleteDC(state->backDc);
        state->backDc = NULL;
    }
}

static void PaintOverlayBuffered(OverlayState *state, HWND hwnd, HDC dc, const RECT &paint) {
    (void)hwnd;
    int w = RectWidth(paint);
    int h = RectHeight(paint);
    if (!state || w <= 0 || h <= 0) {
        return;
    }

    if (EnsureOverlayBackbuffer(state, dc)) {
        RenderOverlayToFullDc(state, state->backDc, paint);
        BitBlt(dc, paint.left, paint.top, w, h, state->backDc, paint.left, paint.top, SRCCOPY);
    } else {
        RECT full = {0, 0, state->capture.w, state->capture.h};
        RenderOverlayToFullDc(state, dc, full);
    }
}

static void UpdateOverlayCursor(OverlayState *state) {
    if (state && state->showModeButtons && !state->dragging && state->hoverMode != -1) {
        SetCursor(state->aeroHoverCursor ? state->aeroHoverCursor : LoadCursor(NULL, IDC_HAND));
    } else if (state && state->aeroCursor) {
        SetCursor(state->aeroCursor);
    } else {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
    }
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    OverlayState *state = reinterpret_cast<OverlayState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        state = reinterpret_cast<OverlayState *>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        UpdateOverlayCursor(state);
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

            if (state->dragging) {
                RECT oldBounds = OverlayVisualBounds(state);
                state->current = {mx, my};
                InvalidateOverlayChange(hwnd, state, oldBounds);
                UpdateWindow(hwnd);
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
    case WM_DESTROY:
        if (state) {
            DestroyOverlayBackbuffer(state);
        }
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool RunSelectionOverlay(HINSTANCE instance, const ScreenCapture &capture, RECT *selection,
                         CaptureKind *kind, bool showModeButtons, CaptureKind initialKind) {
    OverlayState state;
    state.capture = capture;
    state.showModeButtons = showModeButtons;
    state.kind = initialKind;
    state.aeroCursor = CreateAeroGlassCursor(false);
    state.aeroHoverCursor = CreateAeroGlassCursor(true);

    POINT initPt = {};
    if (GetCursorPos(&initPt) && showModeButtons) {
        int mx = initPt.x - capture.x;
        int my = initPt.y - capture.y;
        bool hitMode = false;
        CaptureKind mode = OverlayModeAt(capture.w, capture.h, mx, my, &hitMode);
        if (hitMode) {
            state.hoverMode = (mode == CaptureRegion) ? 0 : 1;
        }
    }

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kOverlayClass, L"Modern Screenshot",
                                WS_POPUP, capture.x, capture.y, capture.w, capture.h,
                                NULL, NULL, instance, &state);
    if (!hwnd) {
        if (state.aeroCursor) DestroyCursor(state.aeroCursor);
        if (state.aeroHoverCursor) DestroyCursor(state.aeroHoverCursor);
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

    DestroyOverlayBackbuffer(&state);
    if (state.aeroCursor) {
        DestroyCursor(state.aeroCursor);
        state.aeroCursor = NULL;
    }
    if (state.aeroHoverCursor) {
        DestroyCursor(state.aeroHoverCursor);
        state.aeroHoverCursor = NULL;
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
