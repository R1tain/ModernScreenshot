#include "long_capture.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace ModernScreenshot {

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

static void ExcludeWindowFromCapture(HWND hwnd) {
    if (!hwnd) return;
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    typedef BOOL(WINAPI * SetAffinityFn)(HWND, DWORD);
    SetAffinityFn setAffinity = reinterpret_cast<SetAffinityFn>(
        reinterpret_cast<void *>(GetProcAddress(user32, "SetWindowDisplayAffinity")));
    if (setAffinity) {
        setAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    }
}

static bool IsOwnWindow(HWND hwnd) {
    while (hwnd) {
        wchar_t className[96] = {};
        GetClassNameW(hwnd, className, 96);
        if (wcscmp(className, kHiddenClass) == 0 || wcscmp(className, kOverlayClass) == 0 ||
            wcscmp(className, kLongSessionClass) == 0 || wcscmp(className, kEditorClass) == 0 ||
            wcscmp(className, kSettingsClass) == 0 || wcscmp(className, kTrayPopupClass) == 0) {
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
    if (GetForegroundWindow() != state->targetRoot) {
        SetForegroundWindow(state->targetRoot);
    }
}

static bool IsCursorOverControlWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    POINT pt = {};
    if (!GetCursorPos(&pt)) return false;
    RECT winRect = {};
    if (!GetWindowRect(hwnd, &winRect)) return false;
    InflateRect(&winRect, 12, 12);
    return PointInRect(pt.x, pt.y, winRect);
}

static bool SendLongScrollStep(LongSessionState *state, bool useMessageFallback) {
    if (!state || !state->target || !IsWindow(state->target)) {
        return false;
    }
    POINT center = {state->selection.left + state->w / 2, state->selection.top + state->h / 2};
    FocusLongTarget(state);

    HWND pointTarget = WindowAtPointForScroll(center, state->target);
    if (pointTarget && IsWindow(pointTarget)) {
        state->target = pointTarget;
        HWND root = RootWindow(pointTarget);
        if (root && !IsOwnWindow(root)) {
            state->targetRoot = root;
        }
    }

    // Keep cursor inside selection only if the user hasn't moved it outside the capture region
    POINT curPt = {};
    if (GetCursorPos(&curPt)) {
        if (PointInRect(curPt.x, curPt.y, state->selection)) {
            center = curPt;
        } else {
            SetCursorPos(center.x, center.y);
        }
    }

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(-WHEEL_DELTA);
    bool sent = SendInput(1, &input, sizeof(input)) == 1;

    if (useMessageFallback && state->target && IsWindow(state->target)) {
        LPARAM lParam = MAKELPARAM(center.x, center.y);
        WPARAM wParam = MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA));
        PostMessageW(state->target, WM_MOUSEWHEEL, wParam, lParam);
        sent = true;
    }
    return sent;
}

static void CheckGlobalSessionHotkeys(HWND hwnd, LongSessionState *state) {
    if (!state || state->done) return;
    bool escDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    bool enterDown = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;

    if (!state->keyDebounceDone) {
        if (!escDown && !enterDown) {
            state->keyDebounceDone = true;
        }
        return;
    }

    if (escDown) {
        state->cancelled = true;
        state->done = true;
        if (hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
    } else if (enterDown) {
        state->done = true;
        if (hwnd && IsWindow(hwnd)) DestroyWindow(hwnd);
    }
}

static void PumpMessagesFor(HWND hwnd, LongSessionState *state, int milliseconds) {
    DWORD start = GetTickCount();
    DWORD waitMs = static_cast<DWORD>(std::max(0, milliseconds));
    MSG msg;
    do {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        CheckGlobalSessionHotkeys(hwnd, state);
        if (!state || state->done || state->cancelled || !IsWindow(hwnd)) {
            break;
        }
        Sleep(10);
    } while ((GetTickCount() - start) < waitMs && IsWindow(hwnd));
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

static inline void ActiveXBounds(int w, int *outX0, int *outX1) {
    int marginL = std::min(12, w / 16);
    int marginR = std::min(22, w / 10); // Exclude right-side scrollbar
    int x0 = std::max(0, marginL);
    int x1 = std::max(x0 + 1, w - marginR);
    *outX0 = x0;
    *outX1 = x1;
}

static inline int PixelLumaAt(const BYTE *rowPtr, int x) {
    const BYTE *p = rowPtr + static_cast<size_t>(x) * 4;
    // Fast integer approximation of Rec.601 luma: (R*77 + G*150 + B*29) >> 8
    return (static_cast<int>(p[2]) * 77 + static_cast<int>(p[1]) * 150 + static_cast<int>(p[0]) * 29) >> 8;
}

static inline int PixelAbsDiffAt(const BYTE *rowA, const BYTE *rowB, int x) {
    const BYTE *a = rowA + static_cast<size_t>(x) * 4;
    const BYTE *b = rowB + static_cast<size_t>(x) * 4;
    return std::abs(static_cast<int>(a[0]) - static_cast<int>(b[0])) +
           std::abs(static_cast<int>(a[1]) - static_cast<int>(b[1])) +
           std::abs(static_cast<int>(a[2]) - static_cast<int>(b[2]));
}

static void ComputeRowSignatures(const std::vector<BYTE> &pixels, int w, int h,
                                 std::vector<RowSignature> *outSigs) {
    if (!outSigs) return;
    outSigs->assign(static_cast<size_t>(std::max(0, h)), RowSignature{});
    if (w <= 0 || h <= 0 || pixels.size() < static_cast<size_t>(w) * static_cast<size_t>(h) * 4) {
        return;
    }

    int x0 = 0, x1 = w;
    ActiveXBounds(w, &x0, &x1);
    int activeW = std::max(1, x1 - x0);
    int xStep = std::max(1, activeW / 96);

    for (int y = 0; y < h; ++y) {
        const BYTE *row = pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4;
        RowSignature &sig = (*outSigs)[static_cast<size_t>(y)];

        int totalLuma = 0;
        int totalSamples = 0;
        int totalGrad = 0;
        int prevLuma = PixelLumaAt(row, x0);

        int binSum[kRowProfileBins] = {};
        int binCount[kRowProfileBins] = {};

        for (int x = x0; x < x1; x += xStep) {
            int lum = PixelLumaAt(row, x);
            totalLuma += lum;
            totalGrad += std::abs(lum - prevLuma);
            prevLuma = lum;
            ++totalSamples;

            int bin = ((x - x0) * kRowProfileBins) / activeW;
            if (bin < 0) bin = 0;
            if (bin >= kRowProfileBins) bin = kRowProfileBins - 1;
            binSum[bin] += lum;
            ++binCount[bin];
        }

        sig.meanLuma = totalSamples > 0 ? (totalLuma / totalSamples) : 0;
        // Scale horizontal gradient so text/icon rows stand out clearly
        sig.horizGrad = totalSamples > 0 ? std::min(255, (totalGrad * 2) / totalSamples) : 0;

        for (int b = 0; b < kRowProfileBins; ++b) {
            sig.bins[b] = static_cast<unsigned char>(
                binCount[b] > 0 ? (binSum[b] / binCount[b]) : sig.meanLuma);
        }
    }

    for (int y = 0; y < h; ++y) {
        RowSignature &sig = (*outSigs)[static_cast<size_t>(y)];
        int nextY = (y + 1 < h) ? (y + 1) : (y > 0 ? y - 1 : 0);
        const RowSignature &nextSig = (*outSigs)[static_cast<size_t>(nextY)];
        int vDiff = std::abs(sig.meanLuma - nextSig.meanLuma);
        for (int b = 0; b < kRowProfileBins; ++b) {
            vDiff += std::abs(static_cast<int>(sig.bins[b]) - static_cast<int>(nextSig.bins[b]));
        }
        sig.vertGrad = std::min(255, vDiff / (kRowProfileBins + 1));
    }
}

static int RowSignatureDiff(const RowSignature &a, const RowSignature &b) {
    int diff = std::abs(a.meanLuma - b.meanLuma) * 2 + std::abs(a.horizGrad - b.horizGrad) * 2;
    for (int i = 0; i < kRowProfileBins; ++i) {
        diff += std::abs(static_cast<int>(a.bins[i]) - static_cast<int>(b.bins[i]));
    }
    return diff / (kRowProfileBins + 4);
}

static int RowPixelDiff(const std::vector<BYTE> &prev, const std::vector<BYTE> &next,
                        int w, int prevY, int nextY) {
    int x0 = 0, x1 = w;
    ActiveXBounds(w, &x0, &x1);
    int activeW = std::max(1, x1 - x0);
    int xStep = std::max(1, activeW / 64);

    const BYTE *rowA = prev.data() + static_cast<size_t>(prevY) * static_cast<size_t>(w) * 4;
    const BYTE *rowB = next.data() + static_cast<size_t>(nextY) * static_cast<size_t>(w) * 4;

    int sum = 0;
    int count = 0;
    for (int x = x0; x < x1; x += xStep) {
        sum += PixelAbsDiffAt(rowA, rowB, x);
        ++count;
    }
    return count > 0 ? (sum / count) : 255;
}

static int AverageFrameDiff(const std::vector<BYTE> &a, const std::vector<BYTE> &b, int w, int h) {
    int x0 = 0, x1 = w;
    ActiveXBounds(w, &x0, &x1);
    int xStep = std::max(1, (x1 - x0) / 36);
    int yStep = std::max(1, h / 36);
    long long diff = 0;
    int samples = 0;
    for (int y = 0; y < h; y += yStep) {
        const BYTE *rowA = a.data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4;
        const BYTE *rowB = b.data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 4;
        for (int x = x0; x < x1; x += xStep) {
            diff += PixelAbsDiffAt(rowA, rowB, x);
            ++samples;
        }
    }
    return samples > 0 ? static_cast<int>(diff / samples) : 255;
}

static bool IsSameFramePixels(const std::vector<BYTE> &a, const std::vector<BYTE> &b) {
    return a.size() == b.size() &&
           (a.empty() || std::memcmp(a.data(), b.data(), a.size()) == 0);
}

static void DetectStaticBorders(const std::vector<BYTE> &prev, const std::vector<BYTE> &next,
                                const std::vector<RowSignature> &prevSig,
                                const std::vector<RowSignature> &nextSig,
                                int w, int h, int *outTopStatic, int *outBotStatic) {
    int maxBorder = std::min(h / 4, 160);

    int topStatic = 0;
    int topTexturedMatches = 0;
    for (int y = 0; y < maxBorder; ++y) {
        int diff = RowPixelDiff(prev, next, w, y, y);
        if (diff <= 4) {
            topStatic = y + 1;
            if (prevSig[static_cast<size_t>(y)].horizGrad >= 4 &&
                nextSig[static_cast<size_t>(y)].horizGrad >= 4) {
                ++topTexturedMatches;
            }
        } else {
            break;
        }
    }
    // Only treat top border as a sticky header if it contains actual stationary UI/text
    if (topTexturedMatches < 2) {
        topStatic = 0;
    }

    int botStatic = 0;
    int botTexturedMatches = 0;
    for (int i = 0; i < maxBorder; ++i) {
        int y = h - 1 - i;
        if (y <= topStatic + 32) break;
        int diff = RowPixelDiff(prev, next, w, y, y);
        if (diff <= 4) {
            botStatic = i + 1;
            if (prevSig[static_cast<size_t>(y)].horizGrad >= 4 &&
                nextSig[static_cast<size_t>(y)].horizGrad >= 4) {
                ++botTexturedMatches;
            }
        } else {
            break;
        }
    }
    if (botTexturedMatches < 2) {
        botStatic = 0;
    }

    *outTopStatic = topStatic;
    *outBotStatic = botStatic;
}

static void VerifyShift2D(const std::vector<BYTE> &prev, const std::vector<BYTE> &next,
                          const std::vector<RowSignature> &prevSig,
                          const std::vector<RowSignature> &nextSig,
                          int w, int h, int shift, int topStatic, int botStatic,
                          int *outOverallError, int *outTexturedError, int *outTexturedRows) {
    int yStart = topStatic + shift;
    int yEnd = h - botStatic;
    int overlapH = yEnd - yStart;
    if (overlapH <= 16) {
        *outOverallError = 255 * 3;
        *outTexturedError = 255 * 3;
        *outTexturedRows = 0;
        return;
    }

    int yStep = std::max(1, overlapH / 96);
    long long totalErr = 0;
    int totalRows = 0;
    long long texErr = 0;
    int texRows = 0;

    for (int prevY = yStart; prevY < yEnd; prevY += yStep) {
        int nextY = prevY - shift;
        int rowErr = RowPixelDiff(prev, next, w, prevY, nextY);
        totalErr += rowErr;
        ++totalRows;

        int grad = std::max(prevSig[static_cast<size_t>(prevY)].horizGrad,
                            nextSig[static_cast<size_t>(nextY)].horizGrad);
        if (grad >= 3) {
            texErr += rowErr;
            ++texRows;
        }
    }

    *outOverallError = totalRows > 0 ? static_cast<int>(totalErr / totalRows) : 255 * 3;
    *outTexturedError = texRows > 0 ? static_cast<int>(texErr / texRows) : *outOverallError;
    *outTexturedRows = texRows;
}

static int FindOptimalSeamRow(const std::vector<BYTE> &prev, const std::vector<BYTE> &next,
                              const std::vector<RowSignature> &prevSig,
                              int w, int h, int shift, int topStatic, int botStatic,
                              int minAllowedPrevY) {
    int overlapStart = std::max(topStatic + shift, minAllowedPrevY);
    int overlapEnd = h - botStatic;
    if (overlapEnd <= overlapStart) {
        return std::min(h, std::max(minAllowedPrevY, h - botStatic));
    }

    // Search in the lower half of the valid overlap (above botStatic) for the cleanest inter-line seam
    int searchWindow = std::min(80, std::max(16, (overlapEnd - overlapStart) / 2));
    int rStart = std::max(overlapStart, overlapEnd - searchWindow);
    int rEnd = overlapEnd;

    int bestRow = rEnd;
    int bestCost = 1000000;

    for (int r = rStart; r < rEnd; ++r) {
        int nextY = r - shift;
        if (nextY < 0 || nextY >= h) continue;
        int matchDiff = RowPixelDiff(prev, next, w, r, nextY);
        const RowSignature &sig = prevSig[static_cast<size_t>(r)];
        // Prefer rows where prev & next match closely AND vertical/horizontal gradients are minimal (whitespace between lines)
        int cost = matchDiff * 4 + sig.vertGrad * 2 + sig.horizGrad;
        if (cost < bestCost) {
            bestCost = cost;
            bestRow = r;
        }
    }
    return bestRow;
}

static int EstimateScrollShift(const std::vector<BYTE> &prev, const std::vector<BYTE> &next,
                               const std::vector<RowSignature> &prevSig,
                               const std::vector<RowSignature> &nextSig,
                               int w, int h, int expectedShift, int minAllowedPrevY,
                               int *outBestError, int *outSeamRow) {
    if (outBestError) *outBestError = 255 * 3;
    if (outSeamRow) *outSeamRow = h;
    if (w <= 0 || h <= 32) return 0;

    int topStatic = 0;
    int botStatic = 0;
    DetectStaticBorders(prev, next, prevSig, nextSig, w, h, &topStatic, &botStatic);

    int activeH = h - topStatic - botStatic;
    int minOverlap = std::max(36, activeH / 6);
    int maxShift = activeH - minOverlap;
    if (maxShift < 2) return 0;

    // 1-pixel resolution 1D signature correlation across every candidate shift
    std::vector<int> sigScore(static_cast<size_t>(maxShift + 1), 255 * 3);
    for (int shift = 2; shift <= maxShift; ++shift) {
        int yStart = topStatic + shift;
        int yEnd = h - botStatic;
        long long weightedDiff = 0;
        long long weightSum = 0;
        int texturedCount = 0;

        for (int prevY = yStart; prevY < yEnd; ++prevY) {
            int nextY = prevY - shift;
            const RowSignature &ps = prevSig[static_cast<size_t>(prevY)];
            const RowSignature &ns = nextSig[static_cast<size_t>(nextY)];
            int rDiff = RowSignatureDiff(ps, ns);
            int grad = std::max(ps.horizGrad, ns.horizGrad);
            int weight = 1 + std::min(16, grad / 3);
            if (grad >= 3) ++texturedCount;
            weightedDiff += static_cast<long long>(rDiff) * weight;
            weightSum += weight;
        }

        if (weightSum > 0) {
            int score = static_cast<int>(weightedDiff / weightSum);
            if (texturedCount < 3) {
                score += 18; // Penalize shifts that only overlap featureless blank space
            }
            sigScore[static_cast<size_t>(shift)] = score;
        }
    }

    // Select up to 8 top local minima from the 1px-resolution 1D score curve for full 2D pixel verification
    struct Candidate {
        int shift;
        int score1D;
    };
    std::vector<Candidate> candidates;
    for (int shift = 2; shift <= maxShift; ++shift) {
        int s = sigScore[static_cast<size_t>(shift)];
        bool isLocalMin = true;
        if (shift > 2 && sigScore[static_cast<size_t>(shift - 1)] < s) isLocalMin = false;
        if (shift < maxShift && sigScore[static_cast<size_t>(shift + 1)] < s) isLocalMin = false;
        if (isLocalMin) {
            candidates.push_back({shift, s});
        }
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) { return a.score1D < b.score1D; });

    if (expectedShift >= 2 && expectedShift <= maxShift) {
        candidates.push_back({expectedShift, sigScore[static_cast<size_t>(expectedShift)]});
    }

    int bestShift = 0;
    int bestVerifiedCost = 255 * 3;
    int bestRawError = 255 * 3;

    int checked = 0;
    for (const Candidate &cand : candidates) {
        if (checked >= 8) break;
        ++checked;

        int overallErr = 255 * 3;
        int texturedErr = 255 * 3;
        int texturedRows = 0;
        VerifyShift2D(prev, next, prevSig, nextSig, w, h, cand.shift,
                      topStatic, botStatic, &overallErr, &texturedErr, &texturedRows);

        int combinedErr = std::max(overallErr, (overallErr + texturedErr) / 2);
        if (texturedRows > 0 && texturedErr > kLongOverlapErrorLimit + 6) {
            continue;
        }

        // Small tie-breaking preference for consistent scroll step on repetitive patterns
        int priorPenalty = 0;
        if (expectedShift > 0) {
            priorPenalty = std::min(3, std::abs(cand.shift - expectedShift) / std::max(8, h / 16));
        }

        int totalCost = combinedErr + priorPenalty;
        if (totalCost < bestVerifiedCost) {
            bestVerifiedCost = totalCost;
            bestRawError = combinedErr;
            bestShift = cand.shift;
        }
    }

    if (outBestError) *outBestError = bestRawError;
    if (bestShift <= 0 || bestRawError > kLongOverlapErrorLimit) {
        return 0;
    }

    // Verify that the scrolled region actually differs from a 0-shift stationary frame
    int zeroOverall = 255 * 3, zeroTex = 255 * 3, zeroRows = 0;
    VerifyShift2D(prev, next, prevSig, nextSig, w, h, 0,
                  topStatic, botStatic, &zeroOverall, &zeroTex, &zeroRows);
    if (zeroOverall <= 1 && bestRawError >= zeroOverall) {
        return 0;
    }

    if (outSeamRow) {
        *outSeamRow = FindOptimalSeamRow(prev, next, prevSig, w, h,
                                         bestShift, topStatic, botStatic, minAllowedPrevY);
    }
    return bestShift;
}

static bool AppendLongFrameWithSeam(LongSessionState *state, HBITMAP bitmap, int shift, int seamRowPrev) {
    if (!state || !bitmap || state->frames.empty() || shift <= 0) return false;
    if (state->totalH >= kLongCaptureMaxHeight ||
        static_cast<int>(state->frames.size()) >= kLongCaptureMaxFrames) {
        state->captureLimitReached = true;
        return false;
    }

    LongFrame &prevFrame = state->frames.back();
    int minSeam = prevFrame.srcY + 4;
    int clampedSeam = Clamp(seamRowPrev, std::min(state->h, minSeam), state->h);

    int trimmedPrevCopyH = clampedSeam - prevFrame.srcY;
    if (trimmedPrevCopyH > 0 && trimmedPrevCopyH < prevFrame.copyH) {
        state->totalH -= (prevFrame.copyH - trimmedPrevCopyH);
        prevFrame.copyH = trimmedPrevCopyH;
    } else {
        clampedSeam = prevFrame.srcY + prevFrame.copyH;
    }

    int newSrcY = Clamp(clampedSeam - shift, 0, state->h - 1);
    int newCopyH = state->h - newSrcY;
    if (state->totalH + newCopyH > kLongCaptureMaxHeight) {
        newCopyH = kLongCaptureMaxHeight - state->totalH;
    }
    if (newCopyH <= 0) return false;

    HBITMAP strip = CopyBitmapStrip(bitmap, state->w, newSrcY, newCopyH);
    if (!strip) return false;

    state->frames.push_back({strip, newSrcY, newCopyH});
    state->totalH += newCopyH;
    state->lastCopyH = shift;
    state->skippedFrames = 0;
    if (state->expectedScrollStep == 0) {
        state->expectedScrollStep = shift;
    } else {
        state->expectedScrollStep = (state->expectedScrollStep * 2 + shift) / 3;
    }
    return true;
}

static bool AppendPendingLongTail(LongSessionState *state) {
    if (!state || !state->pendingBitmap || state->pendingPixels.empty() || state->frames.empty()) {
        return false;
    }

    std::vector<RowSignature> pendingSigs;
    ComputeRowSignatures(state->pendingPixels, state->w, state->h, &pendingSigs);

    int minAllowedPrevY = state->frames.back().srcY + 8;
    int overlapError = 255 * 3;
    int seamRow = state->h;
    int shift = EstimateScrollShift(state->prevPixels, state->pendingPixels,
                                    state->prevSignatures, pendingSigs,
                                    state->w, state->h, state->expectedScrollStep,
                                    minAllowedPrevY, &overlapError, &seamRow);
    if (shift <= 0 || shift >= state->h) {
        return false;
    }

    HBITMAP bitmap = state->pendingBitmap;
    state->pendingBitmap = NULL;
    bool appended = AppendLongFrameWithSeam(state, bitmap, shift, seamRow);
    DeleteObject(bitmap);
    if (!appended) {
        state->pendingPixels.clear();
        state->pendingStableFrames = 0;
        return false;
    }
    state->prevPixels.swap(state->pendingPixels);
    state->prevSignatures.swap(pendingSigs);
    state->pendingPixels.clear();
    state->pendingStableFrames = 0;
    return true;
}

static bool AddLongFrame(LongSessionState *state, HBITMAP bitmap, std::vector<BYTE> *pixels) {
    if (!state || !bitmap || !pixels) return false;
    state->lastSampleUnchanged = false;

    if (!state->hasPrev) {
        ComputeRowSignatures(*pixels, state->w, state->h, &state->prevSignatures);
        state->frames.push_back({bitmap, 0, state->h});
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

    std::vector<RowSignature> currentSigs;
    ComputeRowSignatures(*pixels, state->w, state->h, &currentSigs);

    int minAllowedPrevY = state->frames.back().srcY + 8;
    int overlapError = 255 * 3;
    int seamRow = state->h;
    int shift = EstimateScrollShift(state->prevPixels, *pixels,
                                    state->prevSignatures, currentSigs,
                                    state->w, state->h, state->expectedScrollStep,
                                    minAllowedPrevY, &overlapError, &seamRow);
    if (shift <= 0) {
        if (AverageFrameDiff(state->prevPixels, *pixels, state->w, state->h) <= 2) {
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

    bool appended = AppendLongFrameWithSeam(state, bitmap, shift, seamRow);
    DeleteObject(bitmap);
    if (!appended) return false;
    state->prevPixels.swap(*pixels);
    state->prevSignatures.swap(currentSigs);
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
        PumpMessagesFor(hwnd, state, kLongScrollSettleMs);
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
        if (frame.copyH <= 0) continue;
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
        if (frame.copyH <= 0) continue;
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
        Pen border(Color(95, 255, 255, 255), 1.0f);
        graphics.DrawRectangle(&border, preview.left, preview.top, RectWidth(preview) - 1, RectHeight(preview) - 1);
    }

    RECT badgeRect = {preview.left + 8, preview.top + 8, preview.left + 216, preview.top + 32};
    DrawGlassPanel(dc, badgeRect, 12.0f, true, false);

    wchar_t label[96];
    wsprintfW(label, L"Live preview  •  %d × %d", state->w, state->totalH);
    DrawTextLabel(dc, label, preview.left + 18, preview.top + 12, RGB(248, 250, 252));
}

static void DrawLongSession(HWND hwnd, LongSessionState *state, HDC dc) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    FillRectColor(dc, rect, RGB(14, 18, 26));

    // Ambient content refraction behind the frosted glass panel
    if (state && !state->frames.empty() && state->frames.back().bitmap && state->frames.back().copyH > 0) {
        HDC src = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(src, state->frames.back().bitmap);
        SetStretchBltMode(dc, HALFTONE);
        StretchBlt(dc, 0, 0, RectWidth(rect), RectHeight(rect),
                   src, 0, 0, state->w, state->frames.back().copyH, SRCCOPY);
        SelectObject(src, old);
        DeleteDC(src);
    }

    DrawGlassPanel(dc, rect, 14.0f, true, false);

    wchar_t status[128];
    wsprintfW(status, L"Long capture  •  %d frames  •  %d px", static_cast<int>(state->frames.size()), state->totalH);
    DrawTextLabel(dc, status, 16, 12, RGB(255, 255, 255));

    if (!state->started) {
        DrawTextLabel(dc, L"Preparing automatic scroll capture...", 16, 34, RGB(203, 213, 225));
    } else if (state->captureLimitReached) {
        DrawTextLabel(dc, L"Finished or limit reached. Opening editor.", 16, 34, RGB(251, 191, 36));
    } else if (state->pausedByHover) {
        DrawTextLabel(dc, L"Paused  •  Click Stop (Enter) or Cancel (Esc)", 16, 34, RGB(125, 211, 252));
    } else if (state->skippedFrames > 0) {
        DrawTextLabel(dc, L"Aligning seam overlap...", 16, 34, RGB(251, 191, 36));
    } else if (!state->statusText.empty()) {
        DrawTextLabel(dc, state->statusText, 16, 34, RGB(203, 213, 225));
    } else {
        wchar_t hint[128];
        wsprintfW(hint, L"Auto scrolling  •  +%d px", state->lastCopyH);
        DrawTextLabel(dc, hint, 16, 34, RGB(203, 213, 225));
    }

    RECT preview = {14, 62, 346, 458};
    DrawLongPreview(state, dc, preview);

    DrawTextLabel(dc, L"Enter: Finish   Esc: Cancel", 16, 478, RGB(180, 194, 212));

    RECT done = LongDoneRect();
    RECT cancel = LongCancelRect();
    DrawGlassButton(dc, done, 9.0f, true, false, state->hoveredButton == 1, false, false);
    DrawGlassButton(dc, cancel, 9.0f, false, false, state->hoveredButton == 2, false, false);
    DrawCenteredText(dc, L"Stop", done, -14, FW_SEMIBOLD, RGB(255, 255, 255));
    DrawCenteredText(dc, L"Cancel", cancel, -14, FW_SEMIBOLD, RGB(241, 245, 249));
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
        ExcludeWindowFromCapture(hwnd);
        ApplyWindowGlassStyle(hwnd, 14, 242);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_MOUSEMOVE:
        if (state) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            int hovered = 0;
            if (PointInRect(x, y, LongDoneRect())) hovered = 1;
            else if (PointInRect(x, y, LongCancelRect())) hovered = 2;
            if (hovered != state->hoveredButton) {
                state->hoveredButton = hovered;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
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

    ExcludeWindowFromCapture(hwnd);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd);
    FocusLongTarget(&state);
    SetCursorPos(center.x, center.y);

    state.started = true;
    state.statusText = L"Auto scrolling...";
    InvalidateRect(hwnd, NULL, FALSE);
    PumpMessagesFor(hwnd, &state, 120);

    while (!state.done && !state.cancelled && IsWindow(hwnd)) {
        if (state.totalH >= kLongCaptureMaxHeight ||
            static_cast<int>(state.frames.size()) >= kLongCaptureMaxFrames) {
            state.captureLimitReached = true;
            state.statusText = L"Limit reached.";
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }

        // Pause auto-scrolling while user hovers over the control window so buttons are easy to click
        if (IsCursorOverControlWindow(hwnd)) {
            if (!state.pausedByHover) {
                state.pausedByHover = true;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            PumpMessagesFor(hwnd, &state, 60);
            continue;
        } else if (state.pausedByHover) {
            state.pausedByHover = false;
            InvalidateRect(hwnd, NULL, FALSE);
        }

        bool useFallback = (state.unchangedSteps >= 2);
        if (!SendLongScrollStep(&state, useFallback)) {
            state.statusText = L"Target did not accept scrolling.";
            InvalidateRect(hwnd, NULL, FALSE);
            PumpMessagesFor(hwnd, &state, 400);
            break;
        }

        PumpMessagesFor(hwnd, &state, kLongScrollWaitMs);
        if (state.done || state.cancelled || !IsWindow(hwnd)) break;

        bool added = SampleStableLongSelection(&state, hwnd);
        if (added) {
            state.unchangedSteps = 0;
            wchar_t msg[96];
            wsprintfW(msg, L"Auto scrolling  •  +%d px", state.lastCopyH);
            state.statusText = msg;
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (!state.lastSampleUnchanged) {
            state.unchangedSteps = 0;
            state.statusText = state.pendingStableFrames >= 2 ? L"At bottom or content changed. Verifying..." :
                                                                 L"Aligning seam overlap...";
            InvalidateRect(hwnd, NULL, FALSE);
        } else {
            ++state.unchangedSteps;
            state.statusText = L"Checking for page bottom...";
            InvalidateRect(hwnd, NULL, FALSE);
            if (state.unchangedSteps >= 3) {
                PumpMessagesFor(hwnd, &state, kLongScrollWaitMs * 2);
                if (state.done || state.cancelled || !IsWindow(hwnd)) break;
                SendLongScrollStep(&state, true);
                PumpMessagesFor(hwnd, &state, kLongScrollWaitMs);
                if (state.done || state.cancelled || !IsWindow(hwnd)) break;

                if (SampleStableLongSelection(&state, hwnd)) {
                    state.unchangedSteps = 0;
                    state.statusText = L"Auto scrolling...";
                    InvalidateRect(hwnd, NULL, FALSE);
                    continue;
                }
                if (!state.lastSampleUnchanged) {
                    state.unchangedSteps = 0;
                    state.statusText = L"Aligning seam overlap...";
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
                PumpMessagesFor(hwnd, &state, 350);
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
