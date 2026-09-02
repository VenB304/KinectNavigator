#pragma once
#include "nui_types.h"

// Single-slot latest-frame hand-off between the game's Kinect thread (producer,
// inside the NuiSkeletonGetNextFrame tap) and the recognizer thread (consumer).
//
// We only ever want the newest frame; if the consumer falls behind it simply
// skips whatever it missed. An SRWLOCK + condition variable is ample at 30 Hz.

namespace FrameBuffer
{
    // Producer. Copies `frame` into the slot unless it duplicates the last one
    // (same frame number AND timestamp -- non-blocking polls can hand back a
    // frame we've already seen). Wakes any waiting consumer.
    void Publish(const NUI_SKELETON_FRAME& frame);

    // Consumer. Blocks up to timeoutMs for a sequence newer than *inoutSeq.
    // Returns true and fills `out` (and advances *inoutSeq) when one arrives;
    // false on timeout. Start with *inoutSeq = 0.
    bool WaitLatest(NUI_SKELETON_FRAME& out, unsigned long long& inoutSeq, DWORD timeoutMs);

    // Total frames accepted from producers since load (monotonic).
    unsigned long long Count();

    // Total frames handed to a consumer by WaitLatest (monotonic). The replay
    // tool's lockstep mode waits for this to catch up before publishing the
    // next frame, so nothing is dropped however fast it runs.
    unsigned long long ConsumedCount();
}
