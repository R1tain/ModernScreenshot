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
    FillRectColor(dc, toolbar, RGB(12, 16, 18));

    if (StartGdiplus()) {
        Graphics graphics(dc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        SolidBrush panel(Color(245, 18, 24, 27));
        graphics.FillRectangle(&panel, 0, 0, state->winW, kToolbarH);
        Pen line(Color(120, 80, 96, 98), 1.0f);
        graphics.DrawLine(&line, 0, kToolbarH - 1, state->winW, kToolbarH - 1);
    } else {
        HPEN border = CreatePen(PS_SOLID, 1, RGB(58, 74, 80));
        HGDIOBJ oldPen = SelectObject(dc, border);
        MoveToEx(dc, 0, kToolbarH - 1, NULL);
        LineTo(dc, state->winW, kToolbarH - 1);
        SelectObject(dc, oldPen);
        DeleteObject(border);
    }

    int separatorAfter = ToolbarToolGroupEnd();

    for (int i = 0; i < ButtonCount(); ++i) {
        RECT button = ButtonRect(i);
        bool isTool = IsToolButton(i);
        bool active = isTool && kButtons[i].tool == state->activeTool;
        bool hover = i == state->hoverButton;
        if (StartGdiplus()) {
            Graphics graphics(dc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);
            GraphicsPath path;
            BuildRoundedRectPath(&path, static_cast<float>(button.left), static_cast<float>(button.top),
                                 static_cast<float>(RectWidth(button)),
                                 static_cast<float>(RectHeight(button)), 8.0f);
            Color fillColor = active ? (hover ? Color(255, 34, 60, 64) : Color(255, 28, 48, 52))
                                     : (hover ? Color(255, 30, 40, 46) : Color(255, 21, 28, 31));
            Color outlineColor = active ? Color(255, 23, 198, 163)
                                        : (hover ? Color(255, 138, 168, 170) : Color(120, 82, 98, 100));
            SolidBrush fill(fillColor);
            Pen outline(outlineColor, active ? 1.6f : 1.0f);
            graphics.FillPath(&fill, &path);
            graphics.DrawPath(&outline, &path);
        } else {
            FillRectColor(dc, button, active ? RGB(33, 49, 56) : (hover ? RGB(28, 38, 44) : RGB(22, 29, 34)));
            HPEN pen = CreatePen(PS_SOLID, 1, active ? RGB(23, 198, 163) : RGB(58, 74, 80));
            HGDIOBJ old = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, button.left, button.top, button.right, button.bottom);
            SelectObject(dc, oldBrush);
            SelectObject(dc, old);
            DeleteObject(pen);
        }

        DrawToolbarIcon(dc, kButtons[i].action, button, active);
        RECT labelRect = {button.left, button.bottom - 15, button.right, button.bottom - 2};
        COLORREF labelColor = active ? RGB(23, 198, 163)
                                     : (hover ? RGB(220, 232, 232) : RGB(176, 192, 192));
        DrawCenteredText(dc, kButtons[i].label, labelRect, -10, FW_MEDIUM, labelColor);

        if (i + 1 == separatorAfter && separatorAfter < ButtonCount()) {
            RECT next = ButtonRect(i + 1);
            int sepX = (button.right + next.left) / 2;
            if (StartGdiplus()) {
                Graphics graphics(dc);
                graphics.SetSmoothingMode(SmoothingModeAntiAlias);
                Pen sep(Color(180, 78, 96, 100), 1.0f);
                graphics.DrawLine(&sep, sepX, button.top + 6, sepX, button.bottom - 6);
            } else {
                HPEN sep = CreatePen(PS_SOLID, 1, RGB(78, 96, 100));
                HGDIOBJ oldSep = SelectObject(dc, sep);
                MoveToEx(dc, sepX, button.top + 6, NULL);
                LineTo(dc, sepX, button.bottom - 6);
                SelectObject(dc, oldSep);
                DeleteObject(sep);
            }
        }
    }

    if (!state->statusText.empty() && state->winW > ToolbarWidth() + 120) {
        DrawTextLabel(dc, state->statusText, ToolbarWidth() + 16, 22, RGB(188, 202, 200));
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
    FillRectColor(dc, bg, RGB(12, 16, 18));

    HDC source = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(source, state->shot);
    int visibleH = std::min(state->viewH, state->h - state->scrollY);
    BitBlt(dc, state->imageX, state->imageY, state->w, visibleH, source, 0, state->scrollY, SRCCOPY);
    SelectObject(source, old);
    DeleteDC(source);

    RECT imageBorder = {state->imageX, state->imageY, state->imageX + state->w, state->imageY + state->viewH};
    HPEN border = CreatePen(PS_SOLID, 1, RGB(58, 74, 80));
    HGDIOBJ oldPen = SelectObject(dc, border);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, imageBorder.left, imageBorder.top, imageBorder.right, imageBorder.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(border);

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
        int trackX = state->imageX + state->w - 8;
        int trackTop = state->imageY + 8;
        int trackH = std::max(24, state->viewH - 16);
        float thumbH = std::max(24.0f, static_cast<float>(trackH) * static_cast<float>(state->viewH) / static_cast<float>(state->h));
        float maxThumbTravel = static_cast<float>(trackH) - thumbH;
        float thumbY = static_cast<float>(trackTop);
        if (state->h > state->viewH) {
            thumbY += maxThumbTravel * static_cast<float>(state->scrollY) / static_cast<float>(state->h - state->viewH);
        }
        SolidBrush track(Color(110, 8, 12, 14));
        SolidBrush thumb(Color(210, 23, 198, 163));
        graphics.FillRectangle(&track, static_cast<float>(trackX), static_cast<float>(trackTop), 4.0f, static_cast<float>(trackH));
        graphics.FillRectangle(&thumb, static_cast<float>(trackX - 1), thumbY, 6.0f, thumbH);
    }

    DrawToolbar(state, dc);
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
