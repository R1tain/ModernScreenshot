#pragma once

#include "common.h"
#include "graphics.h"

namespace ModernScreenshot {

struct EditorState {
    HBITMAP shot = NULL;
    HBITMAP stageCache = NULL;
    int stageCacheScrollY = -1;
    HBITMAP compositeCache = NULL;
    size_t compositeCacheOpsCount = 0;
    bool compositeCacheValid = false;
    HCURSOR aeroCursor = NULL;
    HCURSOR aeroHoverCursor = NULL;
    int w = 0;
    int h = 0;
    int winW = 0;
    int winH = 0;
    int imageX = 0;
    int imageY = 0;
    int viewH = 0;
    int scrollY = 0;
    Tool activeTool = ToolRect;
    std::vector<Annotation> ops;
    bool drawing = false;
    Annotation preview;
    bool textEntry = false;
    POINT textPoint = {0, 0};
    std::wstring textBuffer;
    std::wstring statusText;
    bool done = false;
    int hoverButton = -1;
    bool tracking = false;
};

HBITMAP ComposeEditorBitmap(EditorState *state);
bool SaveEditor(EditorState *state, std::wstring *savedPath);
bool CopyEditor(EditorState *state, HWND hwnd);

LRESULT CALLBACK EditorProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

void RunEditorModal(HINSTANCE instance, HBITMAP shot, int w, int h);

} // namespace ModernScreenshot
