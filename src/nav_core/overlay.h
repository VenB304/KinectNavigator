#pragma once
#include "framework.h"
#include "gestures.h"   // GestureDebug

// On-screen HUD: a small always-on-top, click-through, no-activate window that
// shows the recognizer's live state so you can tell whether a gesture is being
// seen while in-game. OFF by default (Cfg::Get().overlay); opt in for
// troubleshooting. Only draws in windowed / borderless -- exclusive fullscreen
// hides it.

namespace Overlay
{
    // idempotent; spawns the UI thread. Respects Cfg::Get().overlay unless
    // force=true (the Lab / Replay dev tools pass true).
    void Start(bool force = false);
    void Stop();

    // Called from the recognizer thread each navigator frame.
    void Update(const GestureDebug& gd, bool bodyTracked, float bodyZ, LONGLONG nowMs);
}
