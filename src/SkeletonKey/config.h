#pragma once
#include "framework.h"

// M3 config. Hardcoded defaults, overridable by a "kinectnav.ini" next to the
// module (key = value, '#' or ';' comments). M4 replaces this with the full
// JSON schema; the field names line up with kinectnav.default.json.

struct Config
{
    bool   enable            = true;
    bool   mirror            = false;   // flip swipe left/right
    bool   requireForeground = true;    // only emit keys when the game window is foreground

    // swipe detector (units: torso-lengths, torso-lengths/sec, frames)
    float  swipeVelocity     = 1.6f;    // min hand speed to start a swipe
    float  swipeVxCeiling    = 8.0f;    // above this = inferred-joint jitter, ignore
    float  swipeDistance     = 0.33f;   // travel in the swipe direction to fire
    int    swipeMinFrames    = 3;
    int    swipeConfirmFrames = 2;      // frames of same-direction speed before the direction latches (anti-windup)

    // hold-to-repeat: after a swipe, if the hand stays out to the right (any
    // height), auto-repeat until it is pulled back toward the body.
    float  holdEnterFx       = 0.60f;   // fast: hand still out past here at end of post-swipe window -> hold
    float  holdExitFx        = 0.45f;   // slow: sustained hand position pulled inside here -> stop
    float  holdDropFy        = -1.10f;  // slow: OR arm fully hanging (below this) -> stop
    int    holdExitFrames    = 4;       // consecutive out-of-zone frames before hold ends (jitter grace)
    int    firstRepeatMs     = 430;
    int    minRepeatMs       = 210;
    int    repeatAccelMs     = 22;      // each repeat shortens the interval by this
    int    postSwipeMs       = 300;     // window after a swipe to decide single vs hold

    // debounce / re-arm  (a wave satisfies re-arm -- no neutral needed between swipes)
    int    cooldownMs        = 160;     // min settle in ReArm before the next swipe
    float  recenterFx        = 0.40f;   // |hand| back inside here -> re-armed
    int    reArmTimeoutMs    = 2500;    // no recenter this long -> fall back to initial arm

    // initial arm (first gesture, and after the sensor loses the body)
    float  armCenterFx       = 0.85f;   // |hand| must be inside here (not parked out to a side)
    int    neutralHoldMs     = 200;
    int    armAfterFrames    = 15;

    // smoothing
    float  smoothFast        = 0.5f;
    float  smoothSlow        = 0.15f;

    // output
    unsigned keyRight        = 0x27;    // VK_RIGHT
    unsigned keyLeft         = 0x25;    // VK_LEFT
    int    keyPressMs        = 40;

    // debug: periodic hand-signal trace to the log (5 Hz of capture time)
    bool   trace             = false;
};

namespace Cfg
{
    // Loads kinectnav.ini from the module directory over the defaults. Idempotent.
    const Config& Load();
    const Config& Get();      // last loaded (defaults if never loaded)
}
