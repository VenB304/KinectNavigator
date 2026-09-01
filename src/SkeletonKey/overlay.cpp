#include "framework.h"
#include <math.h>
#include <stdarg.h>
#include "overlay.h"
#include "config.h"
#include "log.h"

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

    void DrawBar(HDC dc, int x, int y, int w, int h, bool engaged,
                 float val, float dead, float act, COLORREF fill)
    {
        const int   cx    = x + w / 2;
        const float scale = (w / 2.f) / 1.3f;             // 1.3 torso -> bar edge

        HPEN frame = CreatePen(PS_SOLID, 2, RGB(70, 80, 92));
        HGDIOBJ of = SelectObject(dc, frame);
        HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, x, y, x + w, y + h);
        SelectObject(dc, of); SelectObject(dc, ob);
        DeleteObject(frame);

        auto tick = [&](float t, COLORREF col, int wgt) {
            HPEN p = CreatePen(PS_SOLID, wgt, col); HGDIOBJ o = SelectObject(dc, p);
            int px = cx + (int)(t * scale);
            MoveToEx(dc, px, y, nullptr); LineTo(dc, px, y + h);
            px = cx - (int)(t * scale);
            MoveToEx(dc, px, y, nullptr); LineTo(dc, px, y + h);
            SelectObject(dc, o); DeleteObject(p);
        };
        tick(dead, RGB(90, 100, 112), 2);
        tick(act,  RGB(80, 200, 110), 3);

        HPEN mid = CreatePen(PS_SOLID, 1, RGB(110, 120, 132)); HGDIOBJ om = SelectObject(dc, mid);
        MoveToEx(dc, cx, y, nullptr); LineTo(dc, cx, y + h);
        SelectObject(dc, om); DeleteObject(mid);

        if (!engaged) return;
        int vx = cx + (int)(val * scale);
        if (vx < x + 2) vx = x + 2; if (vx > x + w - 2) vx = x + w - 2;
        int r = h / 2 + 2;
        HBRUSH db = CreateSolidBrush(fill);
        HPEN   dp = CreatePen(PS_SOLID, 2, RGB(6, 8, 12));
        HGDIOBJ o1 = SelectObject(dc, db), o2 = SelectObject(dc, dp);
        Ellipse(dc, vx - r, y + h / 2 - r, vx + r, y + h / 2 + r);
        SelectObject(dc, o1); SelectObject(dc, o2);
        DeleteObject(db); DeleteObject(dp);
    }

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

        const COLORREF grey = RGB(170, 180, 190), white = RGB(240, 244, 248);
        const COLORREF green = RGB(90, 225, 130), amber = RGB(240, 195, 70), red = RGB(245, 105, 95);

        HFONT fBig   = MakeFont(H / 8,  true);      // action flash
        HFONT fState = MakeFont(H / 20, true);      // engage state
        HFONT fSmall = MakeFont(H / 40, false);     // corner info + bar labels

        auto blitAndDone = [&]() {
            BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
            DeleteObject(fBig); DeleteObject(fState); DeleteObject(fSmall);
            SelectObject(mem, oldBmp); DeleteObject(bmp);
            DeleteDC(mem); ReleaseDC(hwnd, hdc);
        };

        if (d.inGameplay)
        {
            SelectObject(mem, fState);
            TextOut2(mem, W / 2, 24, L"GAMEPLAY \x2013 muted", grey, TA_CENTER | TA_TOP);
            blitAndDone();
            return;
        }

        // ============ nav_model = extend : air d-pad viz ============
        if (d.dpadMode)
        {
            const int   cx = W / 2, cy = (int)(H * 0.46f);
            const float HF = (float)((W < H ? W : H)) * 0.30f;   // field half-size, px

            // armed-state banner
            SelectObject(mem, fState);
            if (!d.dpadArmed)
                TextOut2(mem, cx, 20, L"ASLEEP  \x2013  hand to shoulder to wake", RGB(240, 195, 70), TA_CENTER | TA_TOP);
            else if (d.dpadParked)
                TextOut2(mem, cx, 20, L"READY", RGB(90, 225, 130), TA_CENTER | TA_TOP);
            const float scale = HF / 1.8f;                        // r = 1.8 torso -> field edge
            const int   px = cx + (int)(d.domEx * scale);
            const int   py = cy - (int)(d.domEy * scale);         // screen y inverted

            HPEN penF = CreatePen(PS_SOLID, 2, RGB(70, 80, 92));
            HPEN penX = CreatePen(PS_SOLID, 1, RGB(70, 80, 92));
            HGDIOBJ oPen = SelectObject(mem, penF);
            HGDIOBJ oBr  = SelectObject(mem, GetStockObject(NULL_BRUSH));

            // field square + crosshair
            Rectangle(mem, cx - (int)HF, cy - (int)HF, cx + (int)HF, cy + (int)HF);
            SelectObject(mem, penX);
            MoveToEx(mem, cx - (int)HF, cy, nullptr); LineTo(mem, cx + (int)HF, cy);
            MoveToEx(mem, cx, cy - (int)HF, nullptr); LineTo(mem, cx, cy + (int)HF);

            // park box
            const int pr = (int)(d.dpadParkR * scale);
            HPEN penP = CreatePen(PS_SOLID, 2, d.dpadParked ? RGB(120, 130, 140) : RGB(80, 90, 100));
            SelectObject(mem, penP);
            HBRUSH brP = d.dpadParked ? CreateSolidBrush(RGB(40, 46, 54)) : (HBRUSH)GetStockObject(NULL_BRUSH);
            HGDIOBJ oBr2 = SelectObject(mem, brP);
            Rectangle(mem, cx - pr, cy - pr, cx + pr, cy + pr);
            SelectObject(mem, oBr2);
            if (d.dpadParked) DeleteObject(brP);
            SelectObject(mem, oPen); SelectObject(mem, oBr);
            DeleteObject(penF); DeleteObject(penX); DeleteObject(penP);

            // direction labels, active wedge lit
            SelectObject(mem, fState);
            const COLORREF liveCol = d.dpadCmd ? RGB(230, 120, 235) : green;
            auto lbl = [&](int wedge, const wchar_t* s, int lx, int ly, UINT al) {
                TextOut2(mem, lx, ly, s, d.dpadWedge == wedge ? liveCol : grey, al);
            };
            lbl(1, L"R \x25B6", cx + (int)HF + 12, cy - H / 40, TA_LEFT | TA_TOP);
            lbl(2, L"\x25C0 L", cx - (int)HF - 12, cy - H / 40, TA_RIGHT | TA_TOP);
            lbl(3, L"\x25B2 U", cx, cy - (int)HF - H / 22, TA_CENTER | TA_TOP);
            lbl(4, L"D \x25BC", cx, cy + (int)HF + 6, TA_CENTER | TA_TOP);

            // hand marker
            COLORREF hm = d.dpadParked ? RGB(150, 160, 170)
                        : d.dpadWedge ? (d.dpadCmd ? RGB(230, 120, 235) : RGB(90, 210, 235))
                        : RGB(210, 180, 90);
            HBRUSH hb = CreateSolidBrush(hm);
            HPEN   hp = CreatePen(PS_SOLID, 2, RGB(6, 8, 12));
            HGDIOBJ o1 = SelectObject(mem, hb), o2 = SelectObject(mem, hp);
            const int hr = H / 90;
            Ellipse(mem, px - hr, py - hr, px + hr, py + hr);
            SelectObject(mem, o1); SelectObject(mem, o2);
            DeleteObject(hb); DeleteObject(hp);

            // text
            SelectObject(mem, fSmall);
            const wchar_t* wn = d.dpadWedge == 1 ? L"RIGHT" : d.dpadWedge == 2 ? L"LEFT"
                              : d.dpadWedge == 3 ? L"UP"    : d.dpadWedge == 4 ? L"DOWN" : L"\x2014";
            wchar_t b[160];
            const bool cmdDir = d.dpadCmd && (d.dpadWedge == 1 || d.dpadWedge == 3);
            swprintf_s(b, L"r %.2f   %s%s", d.armExtend,
                       d.dpadParked ? L"PARKED" : wn,
                       (d.repeatState == 2) ? L"   REPEATING" : L"");
            TextOut2(mem, cx, cy + (int)HF + H / 22, b,
                     d.dpadCmd ? RGB(230, 120, 235) : d.dpadWedge && !d.dpadParked ? green : grey,
                     TA_CENTER | TA_TOP);

            // left-hand -> shoulder gate readout (helps aim it)
            swprintf_s(b, L"L hand \x2192 shoulder  %.2f / %.2f   %s",
                       d.dpadNdDist, d.dpadCmdGateR, d.dpadCmd ? L"COMMAND MODE" : L"");
            TextOut2(mem, cx, cy + (int)HF + H / 22 + H / 34, b,
                     d.dpadCmd ? RGB(230, 120, 235) : (d.dpadNdDist < d.dpadCmdGateR * 1.4f ? amber : grey),
                     TA_CENTER | TA_TOP);

            // command-mode dwell bar
            if (d.dpadCmd && d.dpadWedge && !d.dpadParked && d.repeatState != 2)
            {
                wchar_t cb[48];
                swprintf_s(cb, L"%s  %d%%", cmdDir ? L"CONFIRM" : L"BACK", d.dpadCmdPct);
                TextOut2(mem, cx, cy + (int)HF + H / 22 + 2 * H / 34, cb,
                         d.dpadCmdPct >= 100 ? green : RGB(230, 120, 235), TA_CENTER | TA_TOP);
            }

            if (!s.body || s.bodyZ < 1.2f)
                TextOut2(mem, cx, 20, s.body ? L"TOO CLOSE" : L"NO BODY", amber, TA_CENTER | TA_TOP);

            // action flash
            LONGLONG dage = (d.lastActionMs >= 0) ? (s.nowMs - d.lastActionMs) : -1;
            if (dage >= 0 && dage < 600)
            {
                SelectObject(mem, fBig);
                COLORREF fc = (d.lastAction == 5) ? green : (d.lastAction == 6) ? amber : white;
                TextOut2(mem, W / 2, (int)(H * 0.05f), ActName(d.lastAction), fc, TA_CENTER | TA_TOP);
            }

            blitAndDone();
            return;
        }

        // --- corner: body + dominant hand ---
        SelectObject(mem, fSmall);
        {
            wchar_t b[128];
            if (!s.body)              swprintf_s(b, L"NO BODY");
            else if (s.bodyZ < 1.2f)  swprintf_s(b, L"z %.2f m  TOO CLOSE", s.bodyZ);
            else                      swprintf_s(b, L"z %.2f m", s.bodyZ);
            TextOut2(mem, 24, 20, b, s.body && s.bodyZ >= 1.2f ? grey : amber, TA_LEFT | TA_TOP);

            swprintf_s(b, L"hand  x %+.2f  y %+.2f   v %+.2f,%+.2f", d.domEx, d.domEy, d.domVx, d.domVy);
            TextOut2(mem, 24, 20 + H / 34, b, grey, TA_LEFT | TA_TOP);
            swprintf_s(b, L"arm extend  %d%%   %s (lvl %.1f)",
                       (int)(d.armExtend * 100), d.armLevelOk ? L"level" : L"off-horizontal", d.armLevel);
            TextOut2(mem, 24, 20 + 2 * H / 34, b,
                     (d.armExtend > 0.9f && d.armLevelOk) ? green : grey, TA_LEFT | TA_TOP);

            // Phase 1 parallel signal-conditioning readout (1 Euro pos, SavGol vel)
            swprintf_s(b, L"1\x20ac  x %+.2f  y %+.2f    sg v %+.2f, %+.2f",
                       d.f1eEx, d.f1eEy, d.sgVx, d.sgVy);
            TextOut2(mem, 24, 20 + 3 * H / 34, b, grey, TA_LEFT | TA_TOP);
        }

        // --- status, top centre ---
        SelectObject(mem, fState);
        if (d.repeatState == 2)
            TextOut2(mem, W / 2, 24, L"HOLD-REPEAT  (arm extended)", green, TA_CENTER | TA_TOP);
        else if (d.repeatState == 1)
            TextOut2(mem, W / 2, 24, L"extend arm fully to repeat", amber, TA_CENTER | TA_TOP);
        else if (d.swipeAxis == 1)
            TextOut2(mem, W / 2, 24, L"swiping  \x2190 / \x2192", amber, TA_CENTER | TA_TOP);
        else if (d.swipeAxis == 2)
            TextOut2(mem, W / 2, 24, L"swiping  \x2191 / \x2193", amber, TA_CENTER | TA_TOP);
        else if (d.confirmHeldMs > 0)
        {
            int pct = d.confirmNeedMs > 0 ? d.confirmHeldMs * 100 / d.confirmNeedMs : 0;
            wchar_t b[48]; swprintf_s(b, L"CONFIRM  %d%%", pct > 100 ? 100 : pct);
            TextOut2(mem, W / 2, 24, b, pct >= 100 ? green : amber, TA_CENTER | TA_TOP);
        }
        else if (d.backHeldMs > 0)
        {
            int pct = d.backNeedMs > 0 ? d.backHeldMs * 100 / d.backNeedMs : 0;
            wchar_t b[48]; swprintf_s(b, L"BACK  %d%%", pct > 100 ? 100 : pct);
            TextOut2(mem, W / 2, 24, b, pct >= 100 ? green : amber, TA_CENTER | TA_TOP);
        }
        else
            TextOut2(mem, W / 2, 24, L"ready  \x2013  swipe to navigate", grey, TA_CENTER | TA_TOP);

        // --- action flash, screen centre ---
        LONGLONG age = (d.lastActionMs >= 0) ? (s.nowMs - d.lastActionMs) : -1;
        if (age >= 0 && age < 700)
        {
            SelectObject(mem, fBig);
            COLORREF c = (d.lastAction == 5) ? green : (d.lastAction == 6) ? amber : white;
            TextOut2(mem, W / 2, (int)(H * 0.34f), ActName(d.lastAction), c, TA_CENTER | TA_TOP);
        }

        // --- arm-extend meter, lower third (the repeat trigger) ---
        SelectObject(mem, fSmall);
        {
            int barW = (int)(W * 0.5f), barH = H / 24, barX = (W - barW) / 2, barY = (int)(H * 0.7f);
            HPEN fr = CreatePen(PS_SOLID, 2, RGB(70, 80, 92));
            HGDIOBJ of = SelectObject(mem, fr), ob = SelectObject(mem, GetStockObject(NULL_BRUSH));
            Rectangle(mem, barX, barY, barX + barW, barY + barH);
            SelectObject(mem, of); SelectObject(mem, ob); DeleteObject(fr);
            // extend threshold tick at 90%
            const int tickX = barX + (int)(0.90f * barW);
            HPEN tk = CreatePen(PS_SOLID, 3, RGB(80, 200, 110)); HGDIOBJ ot = SelectObject(mem, tk);
            MoveToEx(mem, tickX, barY, nullptr); LineTo(mem, tickX, barY + barH);
            SelectObject(mem, ot); DeleteObject(tk);
            int fillW = (int)(d.armExtend * barW); if (fillW < 0) fillW = 0; if (fillW > barW) fillW = barW;
            RECT fillR = { barX + 2, barY + 2, barX + fillW - 2, barY + barH - 2 };
            HBRUSH fb = CreateSolidBrush(d.armExtend > 0.9f ? RGB(80, 200, 110) : RGB(120, 130, 145));
            FillRect(mem, &fillR, fb); DeleteObject(fb);
            TextOut2(mem, barX, barY - H / 34, L"ARM EXTENSION  (past the green tick = repeat)", grey, TA_LEFT | TA_TOP);
        }

        BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);

        DeleteObject(fBig); DeleteObject(fState); DeleteObject(fSmall);
        SelectObject(mem, oldBmp); DeleteObject(bmp);
        DeleteDC(mem);
        ReleaseDC(hwnd, hdc);
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
        wc.lpszClassName = L"SkeletonKeyOverlay";
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

void Overlay::Start()
{
    if (s_thread) return;
    if (!Cfg::Get().overlay) return;
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
