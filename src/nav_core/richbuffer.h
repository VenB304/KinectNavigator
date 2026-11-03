#pragma once
#include "rich_types.h"

// Single-slot latest-frame hand-off for KN_RichFrame: the v2 reader thread (producer) to
// the recogniser thread (consumer). Same contract as framebuffer.h, different payload;
// a separate file rather than a template, matching the house style.

namespace RichBuffer
{
    // Producer. Copies `frame` into the slot unless it duplicates the last one (same
    // frame number AND timestamp). Wakes any waiting consumer.
    void Publish(const KN_RichFrame& frame);

    // Consumer. Blocks up to timeoutMs for a sequence newer than *inoutSeq. Returns true
    // and fills `out` (and advances *inoutSeq) when one arrives; false on timeout.
    bool WaitLatest(KN_RichFrame& out, unsigned long long& inoutSeq, unsigned long timeoutMs);

    // Total frames accepted from producers since load (monotonic).
    unsigned long long Count();

    // Total frames handed to a consumer by WaitLatest (monotonic).
    unsigned long long ConsumedCount();
}
