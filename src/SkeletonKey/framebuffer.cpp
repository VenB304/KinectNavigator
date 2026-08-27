#include "framework.h"
#include "framebuffer.h"

namespace
{
    SRWLOCK             g_lock = SRWLOCK_INIT;
    CONDITION_VARIABLE  g_cv   = CONDITION_VARIABLE_INIT;

    NUI_SKELETON_FRAME  g_slot = {};
    unsigned long long  g_seq  = 0;          // bumped on every accepted (non-duplicate) frame
    bool                g_have = false;
    DWORD               g_lastNum = 0;
    LONGLONG            g_lastTs  = 0;
}

void FrameBuffer::Publish(const NUI_SKELETON_FRAME& frame)
{
    AcquireSRWLockExclusive(&g_lock);

    const bool dup = g_have
                  && frame.dwFrameNumber == g_lastNum
                  && frame.liTimeStamp.QuadPart == g_lastTs;
    if (!dup)
    {
        memcpy(&g_slot, &frame, sizeof(g_slot));
        g_lastNum = frame.dwFrameNumber;
        g_lastTs  = frame.liTimeStamp.QuadPart;
        g_have    = true;
        ++g_seq;
    }

    ReleaseSRWLockExclusive(&g_lock);
    if (!dup)
        WakeAllConditionVariable(&g_cv);
}

bool FrameBuffer::WaitLatest(NUI_SKELETON_FRAME& out, unsigned long long& inoutSeq, DWORD timeoutMs)
{
    AcquireSRWLockExclusive(&g_lock);
    while (!g_have || g_seq == inoutSeq)
    {
        if (!SleepConditionVariableSRW(&g_cv, &g_lock, timeoutMs, 0))
        {
            ReleaseSRWLockExclusive(&g_lock);   // timeout (or spurious): nothing new
            return false;
        }
    }
    memcpy(&out, &g_slot, sizeof(out));
    inoutSeq = g_seq;
    ReleaseSRWLockExclusive(&g_lock);
    return true;
}

unsigned long long FrameBuffer::Count()
{
    AcquireSRWLockShared(&g_lock);
    unsigned long long c = g_seq;
    ReleaseSRWLockShared(&g_lock);
    return c;
}
