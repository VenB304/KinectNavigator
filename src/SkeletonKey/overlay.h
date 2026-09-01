#pragma once
#include "framework.h"
#include "gestures.h"   // GestureDebug

// Dev overlay: a small always-on-top, click-through, no-activate window that
// shows the recognizer's live state so you can tell whether a gesture is being
// seen while in-game. Enabled by Cfg::Get().overlay (default on for this build).

namespace Overlay
{
    void Start();   // idempotent; spawns the UI thread. no-op when overlay is off.
    void Stop();

    // Called from the recognizer thread each navigator frame.
    void Update(const GestureDebug& gd, bool bodyTracked, float bodyZ, LONGLONG nowMs);
}
