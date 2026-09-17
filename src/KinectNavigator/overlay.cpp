#include "framework.h"
#include <math.h>
#include <stdarg.h>
#include "overlay.h"
#include "config.h"
#include "log.h"
#include "skeleton_render.h"

// Full-screen layered WS_POPUP window. Colour-key (magenta) => everything that
// isn't drawn on is fully transparent and the game shows through; a mild alpha
// on top of that lets a little game bleed through the HUD text too. It is
// WS_EX_TRANSPARENT (mouse passes through) + WS_EX_NOACTIVATE (never takes
// focus, so the requireForeground key gate keeps working). Repainted ~15 Hz off
// a timer, double-buffered. GDI only. Text is drawn big and outlined so it
// reads from across the room.
//
// Exclusive-fullscreen DirectX will still hide it -- run the game windowed or
// borderless.

namespace
{
    HANDLE           s_thread = nullptr;
    volatile LONG    s_stop   = 0;
    HWND             s_hwnd   = nullptr;
    DWORD            s_tid    = 0;
    CRITICAL_SECTION s_cs;
    bool             s_csInit = false;

    struct Snap
    {
        GestureDebug gd{};
        bool     body  = false;
        float    bodyZ = 0.f;
        LONGLONG nowMs = 0;
    } s_snap;

    const COLORREF KEY = RGB(255, 0, 255);   // colour key -> transparent

    const wchar_t* ActName(unsigned a)
    {
        switch (a)
        {
        case 1: return L"\x2190 LEFT";  case 2: return L"RIGHT \x2192"; case 3: return L"\x2191 UP";
        case 4: return L"DOWN \x2193";  case 5: return L"CONFIRM"; case 6: return L"BACK";
        }
        return L"";
    }

    HFONT MakeFont(int px, bool bold)
    {
        return CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           FF_SWISS, L"Segoe UI");
    }

    // outlined text so it reads over any game content
    void TextOut2(HDC dc, int x, int y, const wchar_t* s, COLORREF fg, UINT align)
    {
        SetTextAlign(dc, align);
        SetTextColor(dc, RGB(6, 8, 12));
        for (int dx = -2; dx <= 2; ++dx)
            for (int dy = -2; dy <= 2; ++dy)
                if (dx || dy) TextOutW(dc, x + dx, y + dy, s, (int)wcslen(s));
        SetTextColor(dc, fg);
        TextOutW(dc, x, y, s, (int)wcslen(s));
    }

    // Skeleton-drawing primitives (palette, ProjectBody, DrawSkeleton, DrawDpadOverlay,
    // RoleStyle) live in skeleton_render.h/.cpp, shared with KinectNavigatorTutorial.
    using namespace SkelRender;

    void Paint(HWND hwnd)
    {
        Snap s;
        EnterCriticalSection(&s_cs); s = s_snap; LeaveCriticalSection(&s_cs);
        const GestureDebug& d = s.gd;

        RECT rc; GetClientRect(hwnd, &rc);
        const int W = rc.right, H = rc.bottom;

        HDC hdc = GetDC(hwnd);
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
        HGDIOBJ oldBmp = SelectObject(mem, bmp);

        HBRUSH bg = CreateSolidBrush(KEY);          // -> transparent
        FillRect(mem, &rc, bg); DeleteObject(bg);
        SetBkMode(mem, TRANSPARENT);

        HFONT fBig = MakeFont(H / 8, true);         // action flash

        auto blitAndDone = [&]() {
            BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
            DeleteObject(fBig);
            SelectObject(mem, oldBmp); DeleteObject(bmp);
            DeleteDC(mem); ReleaseDC(hwnd, hdc);
        };

        // ============ air d-pad HUD (player-facing) ============
        // `d.inGameplay` ("in a song") no longer suppresses navigation -- the d-pad stays
        // fully live -- so the HUD renders as normal, with just a small "IN A SONG" marker
        // to explain the firmer wake hold (dpadArmDwellGameplayMs).
        // Clean dark panel, top-left. Same info as the old dev viz -- big legible
        // state, live d-pad, distance, command gate -- with the raw numbers kept
        // but demoted to one dim line at the bottom. Action fires flash centre-screen.
        if (d.dpadMode)
        {
            const float S = (H / 1080.f) < 0.75f ? 0.75f : (H / 1080.f);
            auto PX = [&](float v) { return (int)(v * S + 0.5f); };
            auto PXn = [&](float v) { int p = PX(v); return p < 1 ? 1 : p; };

            HFONT fSt = MakeFont(PX(23), true);
            HFONT fLb = MakeFont(PX(15), false);
            HFONT fDg = MakeFont(PX(12), false);
            HFONT fRole = MakeFont(PX(12), true);

            // ---- current state ----
            const wchar_t* stT; COLORREF stC; const wchar_t* stS = nullptr;
            if (!s.body)                              { stT = L"STEP INTO VIEW"; stC = cAmb; stS = L"stand centred, face the sensor"; }
            else if (s.bodyZ > 0.f && s.bodyZ < 1.2f) { stT = L"STEP BACK";      stC = cAmb; stS = L"the sensor needs about 2.5 m"; }
            else if (!d.dpadArmed)                    { stT = L"ASLEEP";         stC = cAmb; stS = L"raise a hand to a shoulder to wake"; }
            else if (d.dpadCmd)                       { stT = L"COMMAND";        stC = cMag; }
            else if (d.dpadWedge && !d.dpadParked)    { stT = L"NAVIGATING";     stC = cCyn; }
            else                                     { stT = L"READY";          stC = cGrn; }

            const bool showDwell = d.dpadCmd && d.dpadWedge && !d.dpadParked && d.repeatState != 2;

            // ---- panel geometry ----
            // Figure area is sized for the whole standing body (head to feet, with headroom for
            // a raised hand) rather than the old fixed d-pad glyph box, but the panel keeps the
            // same footprint/position as before -- just a bit taller. Reserved regardless of how
            // many bodies are actually tracked this frame so the panel doesn't resize as people
            // step in/out of frame.
            const int mgn = PX(26), PW = PX(292), padX = PX(15), pTop = PX(13);
            const int figH = PX(230), roleH = PX(16);
            const int PH = pTop
                         + (stS ? PX(46) : PX(31))
                         + figH + roleH + PX(10)
                         + PX(20)                         // metrics row
                         + PX(28)                         // command gate
                         + (showDwell ? PX(22) : 0)
                         + PX(9) + PX(18) + PX(10);       // divider + diag + bottom pad
            const int X0 = mgn, Y0 = mgn;

            HPEN  pnB = CreatePen(PS_SOLID, 1, cBord);
            HBRUSH pnF = CreateSolidBrush(cPanel);
            HGDIOBJ oP = SelectObject(mem, pnB), oB = SelectObject(mem, pnF);
            RoundRect(mem, X0, Y0, X0 + PW, Y0 + PH, PX(16), PX(16));
            SelectObject(mem, oP); SelectObject(mem, oB);
            DeleteObject(pnB); DeleteObject(pnF);

            SetBkMode(mem, TRANSPARENT);
            auto pT = [&](HFONT f, int x, int yy, const wchar_t* str, COLORREF col, UINT al) {
                SelectObject(mem, f); SetTextAlign(mem, al); SetTextColor(mem, col);
                TextOutW(mem, x, yy, str, (int)wcslen(str));
            };
            auto fillR = [&](int x, int yy, int w, int h, COLORREF col) {
                HBRUSH br = CreateSolidBrush(col); RECT r = { x, yy, x + w, yy + h };
                FillRect(mem, &r, br); DeleteObject(br);
            };

            int y = Y0 + pTop;

            // ---- status: dot + label (+ sub) ----
            {
                const int dr = PX(5), dcx = X0 + padX + dr, dcy = y + PX(11);
                HBRUSH db = CreateSolidBrush(stC);
                HGDIOBJ odp = SelectObject(mem, GetStockObject(NULL_PEN)), odb = SelectObject(mem, db);
                Ellipse(mem, dcx - dr, dcy - dr, dcx + dr, dcy + dr);
                SelectObject(mem, odp); SelectObject(mem, odb); DeleteObject(db);
                pT(fSt, X0 + padX + PX(17), y, stT, stC, TA_LEFT | TA_TOP);
                if (d.inGameplay)
                    pT(fDg, X0 + PW - padX, y + PX(4), L"IN A SONG", cAmb, TA_RIGHT | TA_TOP);
                y += PX(25);
                if (stS) { pT(fDg, X0 + padX + PX(17), y, stS, cDim, TA_LEFT | TA_TOP); y += PX(15); }
                y += PX(6);
            }

            // ---- full-skeleton mirror view ----
            // Every tracked body this frame, drawn as a stick figure at its real left/right room
            // position (mirror-adjusted, matching the recognizer's own hand-offset convention --
            // see ProjectBody). Kinect v1 fully tracks at most 2 skeletons' joints at once, so 1
            // column when solo, 2 half-width columns side by side when a second body is present.
            // Only the current driver gets the anatomically-placed d-pad ring + command gate.
            {
                const Config& cc = Cfg::Get();
                const int figTop = y;
                const int nFigs = d.bodyCount > 2 ? 2 : (d.bodyCount < 0 ? 0 : d.bodyCount);
                const int colW = nFigs >= 2 ? PW / 2 : PW;
                const float scale = figH / 5.3f;             // torso -> px; 5.3 = raised-hand-to-feet span + margin
                const int hipY = figTop + (int)(2.4f * scale);

                // left-to-right room order: sort by mirror-adjusted shoulder x, same transform the
                // recogniser applies to hand offsets (gestures.cpp `m = mirror ? -1 : 1`).
                int order[2] = { 0, 1 };
                if (nFigs == 2)
                {
                    const float m = cc.mirror ? -1.f : 1.f;
                    auto shX = [&](int i) {
                        const BodyDebug& bb = d.bodies[i];
                        return m * (bb.joints[NUI_SKELETON_POSITION_SHOULDER_CENTER].x
                                   - bb.joints[NUI_SKELETON_POSITION_HIP_CENTER].x);
                    };
                    if (shX(0) > shX(1)) { order[0] = 1; order[1] = 0; }
                }

                for (int slot = 0; slot < nFigs; ++slot)
                {
                    const BodyDebug& bb = d.bodies[order[slot]];
                    const int cx = X0 + colW * slot + colW / 2;

                    POINT pos[NUI_SKELETON_POSITION_COUNT];
                    ProjectBody(bb, cc.mirror, cx, hipY, scale, pos);

                    COLORREF roleCol; const wchar_t* roleLabel;
                    RoleStyle(bb.role, roleCol, roleLabel);
                    const bool isDriver = (bb.role == 3);   // DpadState::Driving
                    if (isDriver) roleCol = d.dpadCmd ? cMag : (d.dpadWedge && !d.dpadParked ? cCyn : cGrn);

                    DrawSkeleton(mem, bb, pos, roleCol, isDriver ? PXn(3) : PXn(2));
                    if (isDriver) DrawDpadOverlay(mem, pos, d, cc, scale, fLb, PXn(2));

                    pT(fRole, cx, figTop + figH + PX(2), roleLabel, roleCol, TA_CENTER | TA_TOP);
                }

                y = figTop + figH + roleH + PX(10);
            }

            // ---- metrics: heading / distance ----
            {
                const wchar_t* wn = d.dpadWedge == 1 ? L"RIGHT" : d.dpadWedge == 2 ? L"LEFT"
                                  : d.dpadWedge == 3 ? L"UP" : d.dpadWedge == 4 ? L"DOWN" : L"\x2014";
                wchar_t b[64];
                const bool nav = d.dpadWedge && !d.dpadParked;
                swprintf_s(b, L"heading  %s", nav ? wn : L"\x2014");
                pT(fLb, X0 + padX, y, b, nav ? (d.dpadCmd ? cMag : cCyn) : cDim, TA_LEFT | TA_TOP);
                if (s.body && s.bodyZ > 0.f) swprintf_s(b, L"%.1f m", s.bodyZ);
                else                          wcscpy_s(b, L"\x2014");
                pT(fLb, X0 + PW - padX, y, b, (s.body && s.bodyZ >= 2.2f) ? cGrn : cAmb, TA_RIGHT | TA_TOP);
                y += PX(20);
            }

            // ---- command gate bar ----
            {
                pT(fDg, X0 + padX, y, L"left hand \x2192 shoulder", cFnt, TA_LEFT | TA_TOP);
                pT(fDg, X0 + PW - padX, y, d.dpadCmd ? L"on" : L"off", d.dpadCmd ? cMag : cFnt, TA_RIGHT | TA_TOP);
                const int tX = X0 + padX, tW = PW - 2 * padX, tY = y + PX(15), tH = PX(6);
                fillR(tX, tY, tW, tH, cTrk);
                float gf = 1.f - d.dpadNdDist / (d.dpadCmdGateR * 2.f);
                if (gf < 0) gf = 0; if (gf > 1) gf = 1;
                fillR(tX, tY, (int)(tW * gf), tH, d.dpadCmd ? cMag : RGB(0x5F, 0x6B, 0x78));
                y += PX(28);
            }

            // ---- confirm/back dwell bar (only mid-dwell) ----
            if (showDwell)
            {
                const wchar_t* dl = (d.dpadWedge == 1 || d.dpadWedge == 3) ? L"CONFIRM" : L"BACK";
                const COLORREF dc = d.dpadCmdPct >= 100 ? cGrn : cMag;
                wchar_t pb[16]; swprintf_s(pb, L"%d%%", d.dpadCmdPct);
                pT(fDg, X0 + padX, y, dl, dc, TA_LEFT | TA_TOP);
                pT(fDg, X0 + PW - padX, y, pb, dc, TA_RIGHT | TA_TOP);
                const int bX = X0 + padX, bW = PW - 2 * padX, bY = y + PX(14), bH = PX(5);
                fillR(bX, bY, bW, bH, cTrk);
                int fw = (int)(bW * (d.dpadCmdPct / 100.f)); if (fw < 0) fw = 0; if (fw > bW) fw = bW;
                fillR(bX, bY, fw, bH, dc);
                y += PX(22);
            }

            // ---- diagnostic line (raw values, kept for tuning) ----
            {
                HPEN dv = CreatePen(PS_SOLID, 1, cGrid);
                HGDIOBJ odv = SelectObject(mem, dv);
                MoveToEx(mem, X0 + padX, y, nullptr); LineTo(mem, X0 + PW - padX, y);
                SelectObject(mem, odv); DeleteObject(dv);
                y += PX(9);
                const wchar_t* wl = d.dpadWedge == 1 ? L"R" : d.dpadWedge == 2 ? L"L"
                                  : d.dpadWedge == 3 ? L"U" : d.dpadWedge == 4 ? L"D" : L"\x2013";
                wchar_t dg[96];
                swprintf_s(dg, L"r %.2f   wedge %s   rep %d   nd %.2f   z %.2f",
                           d.armExtend, wl, d.repeatState, d.dpadNdDist, s.bodyZ);
                pT(fDg, X0 + padX, y, dg, cFnt, TA_LEFT | TA_TOP);
            }

            // ---- action flash, lower third ----
            LONGLONG dage = (d.lastActionMs >= 0) ? (s.nowMs - d.lastActionMs) : -1;
            if (dage >= 0 && dage < 600)
            {
                SelectObject(mem, fBig);
                const COLORREF fc = (d.lastAction == 5) ? cGrn : (d.lastAction == 6) ? cAmb : cCyn;
                TextOut2(mem, W / 2, (int)(H * 0.78f), ActName(d.lastAction), fc, TA_CENTER | TA_TOP);
            }

            SelectObject(mem, GetStockObject(SYSTEM_FONT));
            DeleteObject(fSt); DeleteObject(fLb); DeleteObject(fDg); DeleteObject(fRole);
            blitAndDone();
            return;
        }

        blitAndDone();   // d.dpadMode is always set on the live path; clean up if it ever isn't
    }

    LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        if (m == WM_TIMER)
        {
            // re-assert topmost -- a fullscreen game shown after us can steal z-order
            SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (m == WM_PAINT) { PAINTSTRUCT ps; BeginPaint(h, &ps); Paint(h); EndPaint(h, &ps); return 0; }
        return DefWindowProcW(h, m, w, l);
    }

    DWORD WINAPI ThreadProc(LPVOID)
    {
        s_tid = GetCurrentThreadId();
        WNDCLASSW wc = {};
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"KinectNavigatorOverlay";
        RegisterClassW(&wc);

        const int sw = GetSystemMetrics(SM_CXSCREEN);
        const int sh = GetSystemMetrics(SM_CYSCREEN);
        s_hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            wc.lpszClassName, L"", WS_POPUP,
            0, 0, sw, sh, nullptr, nullptr, wc.hInstance, nullptr);
        if (!s_hwnd) { LogLine("Overlay: CreateWindowEx failed %lu", GetLastError()); return 1; }

        // magenta -> fully transparent; everything drawn gets alpha 235 so a
        // little of the game bleeds through the HUD too.
        SetLayeredWindowAttributes(s_hwnd, KEY, 235, LWA_COLORKEY | LWA_ALPHA);
        ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
        SetTimer(s_hwnd, 1, 66, nullptr);
        LogLine("Overlay: window up");

        MSG msg;
        while (!s_stop && GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (s_hwnd) { KillTimer(s_hwnd, 1); DestroyWindow(s_hwnd); s_hwnd = nullptr; }
        LogLine("Overlay: thread exit");
        return 0;
    }
}

void Overlay::Start(bool force)
{
    if (s_thread) return;
    if (!force && !Cfg::Get().overlay) return;
    if (!s_csInit) { InitializeCriticalSection(&s_cs); s_csInit = true; }
    InterlockedExchange(&s_stop, 0);
    s_thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
    LogLine("Overlay: Start -> thread=%p", s_thread);
}

void Overlay::Stop()
{
    if (!s_thread) return;
    InterlockedExchange(&s_stop, 1);
    if (s_tid) PostThreadMessageW(s_tid, WM_NULL, 0, 0);   // unblock GetMessage
    WaitForSingleObject(s_thread, 1000);
    CloseHandle(s_thread);
    s_thread = nullptr;
}

void Overlay::Update(const GestureDebug& gd, bool bodyTracked, float bodyZ, LONGLONG nowMs)
{
    if (!s_csInit) return;
    EnterCriticalSection(&s_cs);
    s_snap.gd = gd;
    s_snap.body = bodyTracked;
    s_snap.bodyZ = bodyZ;
    s_snap.nowMs = nowMs;
    LeaveCriticalSection(&s_cs);
}
