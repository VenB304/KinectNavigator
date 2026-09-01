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

        // ============ nav_model = extend : air d-pad HUD (player-facing) ============
        // Clean dark panel, top-left. Same info as the old dev viz -- big legible
        // state, live d-pad, distance, command gate -- with the raw numbers kept
        // but demoted to one dim line at the bottom. Action fires flash centre-screen.
        if (d.dpadMode)
        {
            const float S = (H / 1080.f) < 0.75f ? 0.75f : (H / 1080.f);
            auto PX = [&](float v) { return (int)(v * S + 0.5f); };
            auto PXn = [&](float v) { int p = PX(v); return p < 1 ? 1 : p; };

            const COLORREF cPanel = RGB(0x16, 0x1B, 0x22), cBord = RGB(0x2C, 0x33, 0x3D);
            const COLORREF cTxt = RGB(0xE6, 0xEC, 0xF2), cDim = RGB(0x87, 0x94, 0xA0), cFnt = RGB(0x5F, 0x6B, 0x78);
            const COLORREF cGrn = RGB(0x5C, 0xE0, 0x8A), cAmb = RGB(0xF0, 0xC3, 0x46);
            const COLORREF cCyn = RGB(0x46, 0xD2, 0xEB), cMag = RGB(0xE6, 0x78, 0xEB);
            const COLORREF cGrid = RGB(0x2A, 0x32, 0x3C), cTrk = RGB(0x23, 0x2A, 0x33);

            HFONT fSt = MakeFont(PX(23), true);
            HFONT fLb = MakeFont(PX(15), false);
            HFONT fDg = MakeFont(PX(12), false);

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
            const int mgn = PX(26), PW = PX(292), padX = PX(15), pTop = PX(13);
            const int GP = PX(166);                       // d-pad glyph size
            const int PH = pTop
                         + (stS ? PX(46) : PX(31))
                         + GP + PX(10)
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
                y += PX(25);
                if (stS) { pT(fDg, X0 + padX + PX(17), y, stS, cDim, TA_LEFT | TA_TOP); y += PX(15); }
                y += PX(6);
            }

            // ---- d-pad glyph ----
            {
                const int gx = X0 + PW / 2, gy = y + GP / 2, gh = GP / 2;
                const float sc = gh / 1.8f;                         // r = 1.8 torso -> edge
                const COLORREF live = d.dpadCmd ? cMag : cCyn;

                HPEN gp = CreatePen(PS_SOLID, 1, cGrid);
                HGDIOBJ ogp = SelectObject(mem, gp), ogb = SelectObject(mem, GetStockObject(NULL_BRUSH));
                MoveToEx(mem, gx - gh, gy, nullptr); LineTo(mem, gx + gh, gy);
                MoveToEx(mem, gx, gy - gh, nullptr); LineTo(mem, gx, gy + gh);
                HPEN fp = CreatePen(PS_SOLID, PXn(2), cBord);
                SelectObject(mem, fp);
                RoundRect(mem, gx - gh, gy - gh, gx + gh, gy + gh, PX(12), PX(12));
                DeleteObject(fp);

                const int pr = (int)(d.dpadParkR * sc);
                HPEN pp = CreatePen(PS_SOLID, PXn(2), d.dpadParked ? RGB(0x6E, 0x7C, 0x8A) : RGB(0x4A, 0x55, 0x60));
                HBRUSH pb = d.dpadParked ? CreateSolidBrush(RGB(0x22, 0x28, 0x30)) : (HBRUSH)GetStockObject(NULL_BRUSH);
                SelectObject(mem, pp); HGDIOBJ opb = SelectObject(mem, pb);
                RoundRect(mem, gx - pr, gy - pr, gx + pr, gy + pr, PX(6), PX(6));
                SelectObject(mem, opb); if (d.dpadParked) DeleteObject(pb);
                DeleteObject(pp);
                SelectObject(mem, ogp); SelectObject(mem, ogb); DeleteObject(gp);

                auto chev = [&](int w, const wchar_t* g, int cxk, int cyk, UINT al) {
                    pT(fLb, cxk, cyk, g, d.dpadWedge == w ? live : cFnt, al);
                };
                chev(3, L"\x25B2", gx,              gy - gh - PX(19), TA_CENTER | TA_TOP);
                chev(4, L"\x25BC", gx,              gy + gh + PX(3),  TA_CENTER | TA_TOP);
                chev(1, L"\x25B6", gx + gh + PX(7), gy - PX(9),       TA_LEFT   | TA_TOP);
                chev(2, L"\x25C0", gx - gh - PX(7), gy - PX(9),       TA_RIGHT  | TA_TOP);

                int hx = gx + (int)(d.domEx * sc), hy = gy - (int)(d.domEy * sc);
                const int lim = gh - PX(3);
                if (hx < gx - lim) hx = gx - lim; if (hx > gx + lim) hx = gx + lim;
                if (hy < gy - lim) hy = gy - lim; if (hy > gy + lim) hy = gy + lim;
                const COLORREF hc = d.dpadParked ? RGB(0x96, 0xA0, 0xAA)
                                  : d.dpadWedge  ? live
                                  : RGB(0xD2, 0xB4, 0x5A);
                HBRUSH hbr = CreateSolidBrush(hc);
                HPEN   hpn = CreatePen(PS_SOLID, PXn(2), RGB(0x0B, 0x0F, 0x14));
                HGDIOBJ oh1 = SelectObject(mem, hbr), oh2 = SelectObject(mem, hpn);
                const int hr = PX(6);
                Ellipse(mem, hx - hr, hy - hr, hx + hr, hy + hr);
                SelectObject(mem, oh1); SelectObject(mem, oh2);
                DeleteObject(hbr); DeleteObject(hpn);

                y += GP + PX(10);
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
            DeleteObject(fSt); DeleteObject(fLb); DeleteObject(fDg);
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
