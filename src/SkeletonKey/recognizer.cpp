#include "framework.h"
#include <math.h>
#include "recognizer.h"
#include "framebuffer.h"
#include "gestures.h"
#include "output.h"
#include "overlay.h"
#include "globals.h"
#include "config.h"
#include "log.h"

namespace
{
    HANDLE       g_thread = nullptr;
    volatile LONG g_stop  = 0;

    const char* StateTag(NUI_SKELETON_POSITION_TRACKING_STATE s)
    {
        return s == NUI_SKELETON_POSITION_TRACKED  ? "T"
             : s == NUI_SKELETON_POSITION_INFERRED ? "i"
             : ".";
    }

    // Pick the navigator: prefer the sticky id if it is still tracked, else the
    // closest tracked body, tie-broken by how centred it is. Returns index into
    // frame.SkeletonData or -1 if nobody is tracked.
    int PickNavigator(const NUI_SKELETON_FRAME& f, DWORD stickyId)
    {
        int   best = -1;
        float bestScore = 1e9f;
        int   sticky = -1;

        for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
        {
            const NUI_SKELETON_DATA& s = f.SkeletonData[i];
            if (s.eTrackingState != NUI_SKELETON_TRACKED)
                continue;
            if (stickyId != 0 && s.dwTrackingID == stickyId)
                sticky = i;
            const float score = s.Position.z + 0.001f * fabsf(s.Position.x);
            if (score < bestScore) { bestScore = score; best = i; }
        }
        return sticky >= 0 ? sticky : best;
    }

    void AppendJoint(char* buf, size_t cap, int& len,
                     const NUI_SKELETON_DATA& s, int j, const char* name)
    {
        if (len < 0 || (size_t)len >= cap) return;
        const Vector4& p = s.SkeletonPositions[j];
        int n = sprintf_s(buf + len, cap - len, " %s(%s)%.2f,%.2f,%.2f",
                          name, StateTag(s.eSkeletonPositionTrackingState[j]),
                          p.x, p.y, p.z);
        if (n > 0) len += n;
    }

    // --- menu-vs-gameplay probe: one line ~1/s, whatever the body state -------
    BOOL CALLBACK PickOwnWnd(HWND h, LPARAM lp)
    {
        DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
        if (pid == GetCurrentProcessId() && IsWindowVisible(h) && GetWindow(h, GW_OWNER) == nullptr)
        {
            wchar_t t[8]; if (GetWindowTextLengthW(h) > 0) { GetWindowTextW(h, t, 8); *(HWND*)lp = h; return FALSE; }
        }
        return TRUE;
    }
    HWND GameWnd()
    {
        static HWND s_w = nullptr;
        if (s_w && IsWindow(s_w)) return s_w;
        EnumWindows(PickOwnWnd, (LPARAM)&s_w);
        return s_w;
    }

    void GameProbeTick(const NUI_SKELETON_FRAME& f)
    {
        static LONGLONG s_nextMs = 0, s_prevCalls = 0, s_prevOpens = 0, s_prevTick = 0;
        static DWORD    s_prevTrk = 0xFFFFFFFF;
        const LONGLONG nowTick = (LONGLONG)GetTickCount64();

        if (g_trkId0 != s_prevTrk)
        {
            LogLine("GameProbe/trk: [%lu,%lu]  (was %ld)", g_trkId0, g_trkId1, (long)s_prevTrk);
            s_prevTrk = g_trkId0;
        }
        if (nowTick < s_nextMs) return;
        s_nextMs = nowTick + 1000;

        double cpRate = 0.0, opRate = 0.0;
        if (s_prevTick && nowTick > s_prevTick)
        {
            const double dt = (double)(nowTick - s_prevTick);
            cpRate = (double)(g_colorCalls - s_prevCalls) * 1000.0 / dt;
            opRate = (double)(g_fileOpens  - s_prevOpens) * 1000.0 / dt;
        }
        s_prevCalls = g_colorCalls; s_prevOpens = g_fileOpens; s_prevTick = nowTick;

        int bodies = 0, anyPos = 0;
        for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
        {
            if (f.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED) ++bodies;
            else if (f.SkeletonData[i].eTrackingState == NUI_SKELETON_POSITION_ONLY) ++anyPos;
        }

        wchar_t title[80] = L""; int rw = 0, rh = 0; DWORD wstyle = 0;
        HWND gw = GameWnd();
        if (gw)
        {
            GetWindowTextW(gw, title, 80);
            RECT wr; if (GetWindowRect(gw, &wr)) { rw = wr.right - wr.left; rh = wr.bottom - wr.top; }
            wstyle = (DWORD)GetWindowLongW(gw, GWL_STYLE);
        }
        // "the game is actually up front and full-sized" -- the gameplay-mute needs
        // this so a stale song-load arm can't mute while the window is minimised or
        // alt-tabbed away (open/s drops to 0 there and the idle timer would fire).
        const bool wndActive = gw && !IsIconic(gw) && GetForegroundWindow() == gw
                               && rw >= 640 && rh >= 400;
        InterlockedExchange(&g_gameWndActive, wndActive ? 1 : 0);
        CURSORINFO ci; ci.cbSize = sizeof(ci);
        int cur = GetCursorInfo(&ci) ? (int)((ci.flags & CURSOR_SHOWING) != 0) : -1;
        const LONGLONG mp = g_menuPollTick;

        LogLine("GameProbe: cp=%.0f/s res=%ld trk=[%lu,%lu] bodies=%d/%d cursor=%d "
                "win=%dx%d wact=%ld style=%08lX menupoll=%ldms open/s=%.0f last=\"%ls\" title=\"%ls\"",
                cpRate, g_colorRes, g_trkId0, g_trkId1, bodies, anyPos, cur,
                rw, rh, g_gameWndActive, wstyle, mp ? (long)(nowTick - mp) : -1, opRate, g_lastFile, title);
    }

    DWORD WINAPI ThreadProc(LPVOID)
    {
        LogLine("Recognizer: thread start (tid=%lu)", GetCurrentThreadId());

        NUI_SKELETON_FRAME f;
        unsigned long long seq = 0;
        DWORD      navId    = 0;
        LONGLONG   navSeen  = 0;         // in frame-clock ms
        LONGLONG   lastLog  = 0;
        bool       hadBody  = false;
        unsigned long long ticks = 0;

        while (!g_stop)
        {
            if (!FrameBuffer::WaitLatest(f, seq, 200))
                continue;                       // 200 ms wall wake to re-check g_stop
            ++ticks;

            GameProbeTick(f);                   // ~1/s, runs whether or not a body is tracked

            // All recognizer timing runs off the sensor's own clock so replay at
            // any speed behaves like live. liTimeStamp is milliseconds.
            const LONGLONG now = f.liTimeStamp.QuadPart;
            const int idx = PickNavigator(f, navId);

            if (idx < 0)
            {
                if (hadBody)
                {
                    LogLine("Recognizer: body LOST (frame=%lu)", f.dwFrameNumber);
                    hadBody = false;
                    Gestures::Reset();
                }
                if (navId != 0 && now - navSeen > 500) { navId = 0; }
                GestureDebug gd; Gestures::GetDebug(gd);
                Overlay::Update(gd, false, 0.f, now);
                continue;
            }

            const NUI_SKELETON_DATA& nav = f.SkeletonData[idx];
            navSeen = now;

            if (!hadBody)
            {
                LogLine("Recognizer: body ACQUIRED id=%lu pos=%.2f,%.2f,%.2f",
                        nav.dwTrackingID, nav.Position.x, nav.Position.y, nav.Position.z);
                hadBody = true;
            }
            if (nav.dwTrackingID != navId)
            {
                LogLine("Recognizer: navigator -> id=%lu", nav.dwTrackingID);
                navId = nav.dwTrackingID;
                Gestures::Reset();
            }

            switch (Gestures::Update(nav, now))
            {
            case GestureAction::Right:   Output::TapKey(Cfg::Get().keyRight);   break;
            case GestureAction::Left:    Output::TapKey(Cfg::Get().keyLeft);    break;
            case GestureAction::Up:      Output::TapKey(Cfg::Get().keyUp);      break;
            case GestureAction::Down:    Output::TapKey(Cfg::Get().keyDown);    break;
            case GestureAction::Confirm: Output::TapKey(Cfg::Get().keyConfirm); break;
            case GestureAction::Back:    Output::TapKey(Cfg::Get().keyBack);    break;
            default: break;
            }

            GestureDebug gd; Gestures::GetDebug(gd);
            Overlay::Update(gd, true, nav.Position.z, now);

            if (now - lastLog >= 1000)          // ~1 detail line per second
            {
                lastLog = now;

                int bodies = 0;
                for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
                    if (f.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED) ++bodies;

                int t = 0, in = 0, no = 0;
                for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j)
                {
                    switch (nav.eSkeletonPositionTrackingState[j])
                    {
                    case NUI_SKELETON_POSITION_TRACKED:  ++t;  break;
                    case NUI_SKELETON_POSITION_INFERRED: ++in; break;
                    default:                             ++no; break;
                    }
                }

                char d[1024]; int len = 0; d[0] = '\0';
                AppendJoint(d, sizeof(d), len, nav, NUI_SKELETON_POSITION_HEAD,            "HEAD");
                AppendJoint(d, sizeof(d), len, nav, NUI_SKELETON_POSITION_SHOULDER_CENTER, "SC");
                AppendJoint(d, sizeof(d), len, nav, NUI_SKELETON_POSITION_HAND_LEFT,       "HANDL");
                AppendJoint(d, sizeof(d), len, nav, NUI_SKELETON_POSITION_HAND_RIGHT,      "HANDR");
                AppendJoint(d, sizeof(d), len, nav, NUI_SKELETON_POSITION_HIP_CENTER,      "HIPC");

                LogLine("Recognizer: id=%lu frame=%lu bodies=%d joints T/i/.=%d/%d/%d%s",
                        nav.dwTrackingID, f.dwFrameNumber, bodies, t, in, no, d);
            }
        }

        LogLine("Recognizer: thread exit (ticks=%llu, frames=%llu)", ticks, FrameBuffer::Count());
        return 0;
    }
}

void Recognizer::Start()
{
    if (g_thread) return;
    InterlockedExchange(&g_stop, 0);
    g_thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
    LogLine("Recognizer: Start -> thread=%p", g_thread);
}

void Recognizer::Stop()
{
    if (!g_thread) return;
    InterlockedExchange(&g_stop, 1);
    WaitForSingleObject(g_thread, 800);
    CloseHandle(g_thread);
    g_thread = nullptr;
}
