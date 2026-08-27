#include "framework.h"
#include <math.h>
#include "recognizer.h"
#include "framebuffer.h"
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

            // All recognizer timing runs off the sensor's own clock so replay at
            // any speed behaves like live. liTimeStamp is milliseconds.
            const LONGLONG now = f.liTimeStamp.QuadPart;
            const int idx = PickNavigator(f, navId);

            if (idx < 0)
            {
                if (hadBody) { LogLine("Recognizer: body LOST (frame=%lu)", f.dwFrameNumber); hadBody = false; }
                if (navId != 0 && now - navSeen > 500) { navId = 0; }
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
            }

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
