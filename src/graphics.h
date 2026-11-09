#pragma once

#include "common.h"
#include "raii.h"
#include "utils.h"

namespace ModernScreenshot {

bool StartGdiplus();
void StopGdiplus();
bool GetEncoderClsid(const wchar_t *mimeType, CLSID *clsid);
bool SaveHbitmapPng(HBITMAP bitmap, const std::wstring &path);

ScreenCapture CaptureVirtualScreen();
HBITMAP CropCapture(const ScreenCapture &capture, const RECT &selection);
HBITMAP CaptureScreenRect(const RECT &rect);
HBITMAP CopyBitmapStrip(HBITMAP sourceBitmap, int w, int sourceY, int copyH);
bool ReadBitmapPixels(HBITMAP bitmap, int w, int h, std::vector<BYTE> *pixels);

void FillRectColor(HDC dc, const RECT &rect, COLORREF color);
void DrawTextLabel(HDC dc, const std::wstring &text, int x, int y, COLORREF color);
void DrawCenteredText(HDC dc, const wchar_t *text, const RECT &rect, int fontHeight, int weight, COLORREF color);

// Apple macOS / Windows Aero Frosted Glass primitives
HCURSOR CreateAeroGlassCursor(bool interactiveOrb = false);
void ApplyWindowGlassStyle(HWND hwnd, int cornerRadius, BYTE alpha = 244);
void BlurDcRegion(HDC dc, const RECT &rect, int blurRadius = 12);
void DrawGlassPanel(HDC dc, const RECT &rect, float radius, bool blurBackdrop = true, bool highlight = false);
void DrawGlassButton(HDC dc, const RECT &rect, float radius, bool primary, bool active,
                     bool hovered, bool pressed = false, bool blurBackdrop = false);

void DrawToolbarIcon(HDC dc, Action action, const RECT &button, bool active);
void ApplyMosaic(HDC dc, RECT rect);
void DrawArrow(HDC dc, POINT a, POINT b, COLORREF color);
void DrawAnnotation(HDC dc, const Annotation &op, int offsetX, int offsetY, int maxW, int maxH, bool isPreview = false);

} // namespace ModernScreenshot
