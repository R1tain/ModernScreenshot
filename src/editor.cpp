#include "editor.h"

namespace ModernScreenshot {

void NotifyAction(HWND owner, const std::wstring &title, const std::wstring &message);

static bool IsToolButton(int i) {
    return kButtons[i].action == ActionToolBlur || kButtons[i].action == ActionToolArrow ||
           kButtons[i].action == ActionToolRect || kButtons[i].action == ActionToolText;
}

static int ToolbarToolGroupEnd() {
    for (int i = 0; i < ButtonCount(); ++i) {
        if (!IsToolButton(i)) return i;
    }
    return ButtonCount();
}

static int ToolbarHitButton(int x, int y) {
    if (y < 0 || y >= kToolbarH) return -1;
    for (int i = 0; i < ButtonCount(); ++i) {
        if (PointInRect(x, y, ButtonRect(i))) return i;
    }
    return -1;
}

static bool PointInImage(EditorState *state, int x, int y) {
    return x >= state->imageX && y >= state->imageY &&
           x < state->imageX + state->w && y < state->imageY + state->viewH;
}

static POINT ToImagePoint(EditorState *state, int x, int y) {
    POINT point;
    point.x = Clamp(x - state->imageX, 0, state->w - 1);
    point.y = Clamp(y - state->imageY + state->scrollY, 0, state->h - 1);
    return point;
}

static bool ScrollEditor(EditorState *state, HWND hwnd, int delta) {
    if (!state || state->h <= state->viewH) return false;
    int old = state->scrollY;
    int maxScroll = std::max(0, state->h - state->viewH);
    state->scrollY = Clamp(state->scrollY + delta, 0, maxScroll);
    if (state->scrollY != old) {
        InvalidateRect(hwnd, NULL, FALSE);
        return true;
    }
    return false;
}

static void DrawToolbar(EditorState *state, HDC dc) {
    RECT toolbar = {0, 0, state->winW, kToolbarH};
    FillRectColor(dc, toolbar, RGB(14, 17, 24));

    // Refract top slice of the screenshot through the frosted glass toolbar (macOS vibrancy)
    if (state->shot && state->w > 0 && state->h > 0) {
        HDC source = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(source, state->shot);
        int sampleH = std::min(state->h - state->scrollY, std::max(16, kToolbarH));
        SetStretchBltMode(dc, HALFTONE);
        StretchBlt(dc, 0, 0, state->winW, kToolbarH,
                   source, 0, state->scrollY, state->w, std::max(1, sampleH), SRCCOPY);
        SelectObject(source, old);
        DeleteDC(source);
    }

    DrawGlassPanel(dc, toolbar, 0.0f, true, false);

    int separatorAfter = ToolbarToolGroupEnd();

    for (int i = 0; i < ButtonCount(); ++i) {
        RECT button = ButtonRect(i);
        bool isTool = IsToolButton(i);
        bool active = isTool && kButtons[i].tool == state->activeTool;
        bool hover = i == state->hoverButton;
        bool isPrimaryAction = (kButtons[i].action == ActionCopy || kButtons[i].action == ActionSave) && hover;

        DrawGlassButton(dc, button, 9.0f, isPrimaryAction, active, hover, false, false);
        DrawToolbarIcon(dc, kButtons[i].action, button, active || isPrimaryAction);

        RECT labelRect = {button.left, button.bottom - 15, button.right, button.bottom - 2};
        COLORREF labelColor = (active || isPrimaryAction)
                                  ? RGB(255, 255, 255)
                                  : (hover ? RGB(248, 250, 252) : RGB(203, 213, 225));
        DrawCenteredText(dc, kButtons[i].label, labelRect, -10, (active || isPrimaryAction) ? FW_SEMIBOLD : FW_MEDIUM, labelColor);

        if (i + 1 == separatorAfter && separatorAfter < ButtonCount()) {
            RECT next = ButtonRect(i + 1);
            int sepX = (button.right + next.left) / 2;
            if (StartGdiplus()) {
                Graphics graphics(dc);
                graphics.SetSmoothingMode(SmoothingModeAntiAlias);
                Pen sep(Color(75, 255, 255, 255), 1.0f);
                graphics.DrawLine(&sep, sepX, button.top + 8, sepX, button.bottom - 8);
            } else {
                HPEN sep = CreatePen(PS_SOLID, 1, RGB(64, 74, 94));
                HGDIOBJ oldSep = SelectObject(dc, sep);
                MoveToEx(dc, sepX, button.top + 8, NULL);
                LineTo(dc, sepX, button.bottom - 8);
                SelectObject(dc, oldSep);
                DeleteObject(sep);
            }
        }
    }

    if (!state->statusText.empty() && state->winW > ToolbarWidth() + 120) {
        DrawTextLabel(dc, state->statusText, ToolbarWidth() + 16, 22, RGB(226, 232, 240));
    }
}

HBITMAP ComposeEditorBitmap(EditorState *state) {
    HDC screen = GetDC(NULL);
    HDC source = CreateCompatibleDC(screen);
    HDC dest = CreateCompatibleDC(screen);
    HBITMAP output = CreateCompatibleBitmap(screen, state->w, state->h);
    HGDIOBJ oldSource = SelectObject(source, state->shot);
    HGDIOBJ oldDest = SelectObject(dest, output);
    BitBlt(dest, 0, 0, state->w, state->h, source, 0, 0, SRCCOPY);

    for (const Annotation &op : state->ops) {
        DrawAnnotation(dest, op, 0, 0, state->w, state->h);
    }

    SelectObject(source, oldSource);
    SelectObject(dest, oldDest);
    DeleteDC(source);
    DeleteDC(dest);
    ReleaseDC(NULL, screen);
    return output;
}

bool SaveEditor(EditorState *state, std::wstring *savedPath) {
    HBITMAP output = ComposeEditorBitmap(state);
    if (!output) return false;
    std::wstring path = BuildScreenshotPath();
    bool ok = SaveHbitmapPng(output, path);
    if (ok && savedPath) {
        *savedPath = path;
    }
    DeleteObject(output);
    return ok;
}

bool CopyEditor(EditorState *state, HWND hwnd) {
    HBITMAP output = ComposeEditorBitmap(state);
    if (!output) return false;
    if (!OpenClipboard(hwnd)) {
        DeleteObject(output);
        return false;
    }
    EmptyClipboard();
    if (!SetClipboardData(CF_BITMAP, output)) {
        CloseClipboard();
        DeleteObject(output);
        return false;
    }
    CloseClipboard();
    return true;
}

static void DrawEditor(EditorState *state, HWND hwnd, HDC dc) {
    RECT bg = {0, 0, state->winW, state->winH};
    FillRectColor(dc, bg, RGB(14, 17, 24));

    HDC source = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(source, state->shot);
    int visibleH = std::min(state->viewH, state->h - state->scrollY);

    // If the window is wider than the screenshot, fill side margins with frosted ambient refraction
    if (state->imageX > 0 && visibleH > 0) {
        SetStretchBltMode(dc, HALFTONE);
        StretchBlt(dc, 0, state->imageY, state->winW, state->viewH,
                   source, 0, state->scrollY, state->w, visibleH, SRCCOPY);
        RECT canvasBg = {0, state->imageY, state->winW, state->winH};
        DrawGlassPanel(dc, canvasBg, 0.0f, true, false);
    }

    BitBlt(dc, state->imageX, state->imageY, state->w, visibleH, source, 0, state->scrollY, SRCCOPY);
    SelectObject(source, old);
    DeleteDC(source);

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        Pen glassFrame(Color(85, 255, 255, 255), 1.0f);
        graphics.DrawRectangle(&glassFrame, state->imageX, state->imageY, state->w - 1, state->viewH - 1);
    }

    int savedClip = SaveDC(dc);
    IntersectClipRect(dc, state->imageX, state->imageY, state->imageX + state->w, state->imageY + state->viewH);
    for (const Annotation &op : state->ops) {
        DrawAnnotation(dc, op, state->imageX, state->imageY - state->scrollY, state->w, state->h);
    }
    if (state->drawing) {
        DrawAnnotation(dc, state->preview, state->imageX, state->imageY - state->scrollY, state->w, state->h);
    }
    if (state->textEntry) {
        Annotation text;
        text.tool = ToolText;
        text.a = state->textPoint;
        text.b = state->textPoint;
        text.text = state->textBuffer + L"_";
        DrawAnnotation(dc, text, state->imageX, state->imageY - state->scrollY, state->w, state->h);
    }
    RestoreDC(dc, savedClip);

    if (state->h > state->viewH && StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        int trackX = state->imageX + state->w - 10;
        int trackTop = state->imageY + 8;
        int trackH = std::max(24, state->viewH - 16);
        float thumbH = std::max(28.0f, static_cast<float>(trackH) * static_cast<float>(state->viewH) / static_cast<float>(state->h));
        float maxThumbTravel = static_cast<float>(trackH) - thumbH;
        float thumbY = static_cast<float>(trackTop);
        if (state->h > state->viewH) {
            thumbY += maxThumbTravel * static_cast<float>(state->scrollY) / static_cast<float>(state->h - state->viewH);
        }
        GraphicsPath trackPath, thumbPath;
        BuildRoundedRectPath(&trackPath, static_cast<float>(trackX), static_cast<float>(trackTop), 6.0f, static_cast<float>(trackH), 3.0f);
        BuildRoundedRectPath(&thumbPath, static_cast<float>(trackX), thumbY, 6.0f, thumbH, 3.0f);
        SolidBrush track(Color(95, 15, 18, 28));
        SolidBrush thumb(Color(210, 255, 255, 255));
        Pen thumbBorder(Color(120, 86, 148, 255), 1.0f);
        graphics.FillPath(&track, &trackPath);
        graphics.FillPath(&thumb, &thumbPath);
        graphics.DrawPath(&thumbBorder, &thumbPath);
    }

    DrawToolbar(state, dc);

    // Outer glass window rim
    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        GraphicsPath winPath;
        BuildRoundedRectPath(&winPath, 0.5f, 0.5f, static_cast<float>(state->winW - 1), static_cast<float>(state->winH - 1), 12.0f);
        Pen winRim(Color(110, 255, 255, 255), 1.0f);
        graphics.DrawPath(&winRim, &winPath);
    }
    (void)hwnd;
}

static void PaintEditorBuffered(EditorState *state, HWND hwnd, HDC dc, const RECT &paint) {
    int w = RectWidth(paint);
    int h = RectHeight(paint);
    if (!state || w <= 0 || h <= 0) return;

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP buffer = CreateCompatibleBitmap(dc, w, h);
    if (!memory || !buffer) {
        if (buffer) DeleteObject(buffer);
        if (memory) DeleteDC(memory);
        DrawEditor(state, hwnd, dc);
        return;
    }

    HGDIOBJ old = SelectObject(memory, buffer);
    int saved = SaveDC(memory);
    SetViewportOrgEx(memory, -paint.left, -paint.top, NULL);
    DrawEditor(state, hwnd, memory);
    RestoreDC(memory, saved);
    BitBlt(dc, paint.left, paint.top, w, h, memory, 0, 0, SRCCOPY);
    SelectObject(memory, old);
    DeleteObject(buffer);
    DeleteDC(memory);
}

static void CommitText(EditorState *state) {
    if (state->textBuffer.empty()) {
        state->textEntry = false;
        return;
    }
    Annotation text;
    text.tool = ToolText;
    text.a = state->textPoint;
    text.b = state->textPoint;
    text.text = state->textBuffer;
    state->ops.push_back(text);
    state->textBuffer.clear();
    state->textEntry = false;
}

static void FinishDrawing(EditorState *state, POINT point) {
    state->preview.b = point;
    RECT rect = NormalizeRectPoints(state->preview.a, state->preview.b);
    bool keep = false;
    if (state->preview.tool == ToolArrow) {
        int dx = state->preview.b.x - state->preview.a.x;
        int dy = state->preview.b.y - state->preview.a.y;
        keep = dx * dx + dy * dy > 25;
    } else {
        keep = RectWidth(rect) >= 4 && RectHeight(rect) >= 4;
    }

    if (keep) {
        state->ops.push_back(state->preview);
    }
    state->drawing = false;
}

static void HandleAction(EditorState *state, HWND hwnd, Action action, Tool tool) {
    if (action == ActionToolBlur || action == ActionToolArrow || action == ActionToolRect || action == ActionToolText) {
        state->activeTool = tool;
        state->textEntry = false;
        state->statusText.clear();
    } else if (action == ActionUndo) {
        if (!state->ops.empty()) {
            state->ops.pop_back();
        }
        state->textEntry = false;
        state->statusText = L"Undo";
    } else if (action == ActionSave) {
        std::wstring path;
        if (SaveEditor(state, &path)) {
            NotifyAction(hwnd, L"Screenshot saved", path);
            state->done = true;
            DestroyWindow(hwnd);
        } else {
            state->statusText = L"Save failed";
            NotifyAction(hwnd, L"Save failed", L"Could not write the screenshot file.");
        }
    } else if (action == ActionCopy) {
        if (CopyEditor(state, hwnd)) {
            NotifyAction(hwnd, L"Screenshot copied", L"Image copied to clipboard.");
            state->done = true;
            DestroyWindow(hwnd);
        } else {
            state->statusText = L"Copy failed";
            NotifyAction(hwnd, L"Copy failed", L"Could not write the image to clipboard.");
        }
    }
}

LRESULT CALLBACK EditorProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    EditorState *state = reinterpret_cast<EditorState *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        ApplyWindowGlassStyle(hwnd, 12, 248);
        SetFocus(hwnd);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    case WM_KEYDOWN:
        if (!state) return 0;
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'Z') {
            if (!state->ops.empty()) state->ops.pop_back();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'S') {
            HandleAction(state, hwnd, ActionSave, ToolBlur);
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'C') {
            HandleAction(state, hwnd, ActionCopy, ToolBlur);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (wParam == VK_NEXT) {
            ScrollEditor(state, hwnd, std::max(80, state->viewH - 80));
            return 0;
        }
        if (wParam == VK_PRIOR) {
            ScrollEditor(state, hwnd, -std::max(80, state->viewH - 80));
            return 0;
        }
        if (wParam == VK_HOME) {
            ScrollEditor(state, hwnd, -state->h);
            return 0;
        }
        if (wParam == VK_END) {
            ScrollEditor(state, hwnd, state->h);
            return 0;
        }
        if (state->textEntry) {
            if (wParam == VK_RETURN) {
                CommitText(state);
            } else if (wParam == VK_BACK && !state->textBuffer.empty()) {
                state->textBuffer.pop_back();
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (wParam == 'B') state->activeTool = ToolBlur;
        else if (wParam == 'A') state->activeTool = ToolArrow;
        else if (wParam == 'R') state->activeTool = ToolRect;
        else if (wParam == 'T') state->activeTool = ToolText;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_CHAR:
        if (state && state->textEntry && wParam >= 32 && wParam != 127 && state->textBuffer.size() < 120) {
            state->textBuffer.push_back(static_cast<wchar_t>(wParam));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (!state) return 0;
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            Tool tool = ToolBlur;
            Action action = ButtonAt(x, y, &tool);
            if (action != ActionNone) {
                HandleAction(state, hwnd, action, tool);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (PointInImage(state, x, y)) {
                POINT point = ToImagePoint(state, x, y);
                if (state->activeTool == ToolText) {
                    if (state->textEntry) CommitText(state);
                    state->textEntry = true;
                    state->textPoint = point;
                    state->textBuffer.clear();
                    InvalidateRect(hwnd, NULL, FALSE);
                } else {
                    if (state->textEntry) CommitText(state);
                    state->drawing = true;
                    state->preview.tool = state->activeTool;
                    state->preview.a = point;
                    state->preview.b = point;
                    state->preview.text.clear();
                    SetCapture(hwnd);
                }
            } else if (y < kToolbarH) {
                SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state) {
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            if (state->drawing) {
                POINT point = ToImagePoint(state, mx, my);
                state->preview.b = point;
                InvalidateRect(hwnd, NULL, FALSE);
            } else {
                int over = ToolbarHitButton(mx, my);
                if (over != state->hoverButton) {
                    state->hoverButton = over;
                    RECT toolbarRect = {0, 0, state->winW, kToolbarH};
                    InvalidateRect(hwnd, &toolbarRect, FALSE);
                }
                if (!state->tracking) {
                    TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, hwnd, 0};
                    if (TrackMouseEvent(&track)) {
                        state->tracking = true;
                    }
                }
            }
        }
        return 0;
    case WM_MOUSELEAVE:
        if (state) {
            state->tracking = false;
            if (state->hoverButton != -1) {
                state->hoverButton = -1;
                RECT toolbarRect = {0, 0, state->winW, kToolbarH};
                InvalidateRect(hwnd, &toolbarRect, FALSE);
            }
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (state) {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (ScrollEditor(state, hwnd, delta > 0 ? -240 : 240)) {
                return 0;
            }
        }
        break;
    case WM_LBUTTONUP:
        if (state && state->drawing) {
            ReleaseCapture();
            POINT point = ToImagePoint(state, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            FinishDrawing(state, point);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (state) {
            PaintEditorBuffered(state, hwnd, dc, ps.rcPaint);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        if (state) state->done = true;
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void RunEditorModal(HINSTANCE instance, HBITMAP shot, int w, int h) {
    EditorState state;
    state.shot = shot;
    state.w = w;
    state.h = h;

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int maxWinH = std::max(kToolbarH + 160, vh - kEditorMargin);
    state.viewH = std::min(h, maxWinH - kToolbarH);
    state.winW = std::max(w, ToolbarWidth());
    state.winH = state.viewH + kToolbarH;
    state.imageX = (state.winW - w) / 2;
    state.imageY = kToolbarH;
    int x = vx + std::max(0, (vw - state.winW) / 2);
    int y = vy + std::max(0, (vh - state.winH) / 2);

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kEditorClass, L"ModernScreenshot",
                                WS_POPUP, x, y, state.winW, state.winH,
                                NULL, NULL, instance, &state);
    if (!hwnd) return;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    MSG msg;
    while (!state.done && GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

} // namespace ModernScreenshot
