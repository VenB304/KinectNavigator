#pragma once
#include "nui_types.h"

// M3: one detector -- a horizontal right-hand swipe -> RIGHT -- behind the full
// debounce state machine (neutral gate, arm delay, cooldown, re-neutral).
// M4 adds the rest of the vocabulary.

enum class GestureAction
{
    None = 0,
    Right,
    Left,
};

namespace Gestures
{
    // Called once per navigator frame by the recognizer thread (single thread,
    // no locking). frameMs is frame.liTimeStamp.QuadPart.
    GestureAction Update(const NUI_SKELETON_DATA& nav, LONGLONG frameMs);

    // Clear all state -- on body lost or navigator change.
    void Reset();
}
