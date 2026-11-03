#include "framework.h"
#include "richbuffer.h"

namespace
{
    SRWLOCK             g_lock = SRWLOCK_INIT;
    CONDITION_VARIABLE  g_cv   = CONDITION_VARIABLE_INIT;

    KN_RichFrame        g_slot = {};
    unsigned long long  g_seq  = 0;          // bumped on every accepted (non-duplicate) frame
    unsigned long long  g_consumed = 0;      // bumped each time WaitLatest hands a frame out
    bool                g_have = false;
    unsigned int        g_lastNum = 0;
    long long           g_lastTs  = 0;
}

void RichBuffer::Publish(const KN_RichFrame& frame)
{
    AcquireSRWLockExclusive(&g_lock);

    const bool dup = g_have
                  && frame.frameNumber == g_lastNum
                  && frame.timestampMs == g_lastTs;
    if (!dup)
    {
        memcpy(&g_slot, &frame, sizeof(g_slot));
        g_lastNum = frame.frameNumber;
        g_lastTs  = frame.timestampMs;
        g_have    = true;
        ++g_seq;
    }

    ReleaseSRWLockExclusive(&g_lock);
    if (!dup)
        WakeAllConditionVariable(&g_cv);
}

bool RichBuffer::WaitLatest(KN_RichFrame& out, unsigned long long& inoutSeq, unsigned long timeoutMs)
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
    ++g_consumed;
    ReleaseSRWLockExclusive(&g_lock);
    return true;
}

unsigned long long RichBuffer::Count()
{
    AcquireSRWLockShared(&g_lock);
    unsigned long long c = g_seq;
    ReleaseSRWLockShared(&g_lock);
    return c;
}

unsigned long long RichBuffer::ConsumedCount()
{
    AcquireSRWLockShared(&g_lock);
    unsigned long long c = g_consumed;
    ReleaseSRWLockShared(&g_lock);
    return c;
}
