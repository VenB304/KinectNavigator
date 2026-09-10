#pragma once
#include "framework.h"

// Config. Hardcoded defaults, overridable by a "kinectnav.ini" next to the
// module (key = value, '#' or ';' comments -- see dist/kinectnav.example.ini).
// The .ini key names are the snake_case of these fields (see config.cpp).
//
// Distances are torso units (torso = |SHOULDER_CENTER - HIP_CENTER|, ~0.41 m);
// speeds torso/second; times in ms of the sensor frame-clock.
//
// One recogniser: an air d-pad centred on the dominant SHOULDER -- reach the arm
// into a direction wedge past a park box, hold for auto-repeat. Non-dominant hand
// on its shoulder = command mode (reach = Enter/Esc). The `dpad_*` fields.
// (An older swipe-based model was removed in v1.1.)

struct Config
{
    bool   enable            = true;
    bool   mirror            = false;   // flip left/right output
    bool   leftHanded        = false;   // dominant hand = LEFT
    bool   enableConfirm     = true;
    bool   enableBack        = true;
    bool   requireForeground = true;    // only emit keys while the game window is foreground

    // --- air d-pad centred on the dominant shoulder (frontal plane, torso units;
    // +x = outward on the dominant side after `mirror`, +y = up) ---
    // Clutch: starts DISARMED. Park the hand at the shoulder (dpadArmDwellMs) to arm it.
    // It auto-disarms after the hand sits in no-man's-land -- not parked and not in any wedge
    // (arm lowered / between gestures) -- for dpadDisarmMs continuously (any wedge or park
    // touch resets the timer). No motion/dance detection. Re-arm = park again.
    bool   dpadArm           = true;    // 0 = always live (no clutch)
    int    dpadArmDwellMs    = 350;     // hold the hand in the park box this long to arm -- long
                                        // enough that an incidental brush past the shoulder doesn't wake it
    int    dpadArmDwellGameplayMs = 500; // ...but while a song is playing (suppressInGame + file-quiet;
                                        // suppressInGame is OFF by default), a firmer hold -- guards
                                        // against an accidental wake -> ESC mid-routine
    int    dpadDisarmMs      = 500;     // ...hand idle out of park & out of every wedge this long -> disarm
    float  dpadSleepBelowY   = -0.35f;  // ...but only while the hand is at least this far below the
                                        // dom shoulder (torso). A hand raised in the dead gap between
                                        // wedge cones is "aiming" (UP / Confirm setup), not "done".

    // Dance auto-disarm: while armed, if the whole body is moving a lot (trunk + head speed,
    // torso/s, EMA-smoothed) for dpadDanceHoldMs continuously -> disarm. The dominant hand is
    // excluded (it's supposed to move for navigation); a person standing and reaching stays
    // well under the threshold, a dancer does not.
    bool   dpadDanceDisarm   = false;   // off by default: in-game it false-trips on vigorous
                                        // navigation (trunk lean) far more than it catches dancing
    float  dpadDanceEnergy   = 2.0f;    // weighted trunk/head speed above this = "moving a lot"
    int    dpadDanceHoldMs   = 500;     // ...sustained this long -> disarm ("dpad disarmed (dancing)")

    float  dpadParkRadius    = 0.32f;   // |hand - dom shoulder| under this = parked / home / off
    float  dpadParkExitK     = 1.35f;   // must reach parkRadius * this to LEAVE park (hysteresis)
    // Anisotropic park-exit: scale one axis of the hand offset in the park-exit test only (wedges
    // and the HUD still use the raw offset). k = 1.0 is symmetric / off.
    float  dpadUpReachK      = 0.85f;   // ey > 0: park-exit sees ey * k. k < 1 -> UP must raise
                                        // further before it leaves park -> less eager.
    float  dpadCrossReachK   = 1.30f;   // ex < 0 (cross-body): park-exit sees ex * k. k > 1 -> LEFT
                                        // leaves park at a shorter cross-body reach -> easier to hit.
    float  dpadWedgeHalfDeg  = 24.0f;   // half-angle of the R/L/U wedges; the gaps between are dead
    float  dpadWedgeTanHalf  = 0.4452f; // tan(dpadWedgeHalfDeg) -- precomputed in Cfg::Load, not an ini key
    float  dpadDownMinDrop   = 0.25f;   // DOWN: hand at least this far below the shoulder ...
    float  dpadDownMinOut    = 0.30f;   // ... AND this far out sideways (a hanging arm is ~0 -> ignored)
    int    dpadEntryDebounce = 3;       // frames continuously in a wedge before the single press fires
    int    dpadRepeatDwellMs = 600;     // ...then hold the wedge this long for auto-repeat to begin
    int    dpadRepeatFirstMs = 430;     // first repeat interval (nav), accelerates ...
    int    dpadRepeatMinMs   = 200;     // ... down to this
    int    dpadRepeatAccelMs = 22;      // each repeat shortens the interval by this
    int    dpadWedgeGraceFr  = 4;       // tolerate this many frames of jitter into a dead gap
    float  dpadCmdGateRadius = 0.42f;   // NON-dom hand within this of the NON-dom shoulder = command mode
                                        // (the hand is usually INFERRED there -- keep it generous)
    float  dpadCmdGateExitK  = 1.5f;    // ...once engaged, stays until the hand is radius*this away (hysteresis)
    int    dpadCmdDwellMs    = 600;     // command mode: hold up/right this long before ENTER fires
                                        // (nav single-presses stay on the fast dpadEntryDebounce)
    int    dpadBackDwellMs   = 1500;    // ...but hold down/left this long before ESC fires (Esc is
                                        // consequential -- pause/exit -- so it wants a firm hold)
    int    dpadCmdRepeatMs   = 700;     // command-mode (Enter/Esc) slow repeat interval
    int    dpadHandoffGraceMs = 500;    // multiplayer: after control passes to a new driver, ignore
                                        // further hand-offs this long (anti-thrash between two people)

    // "in a song" detection via the game's file I/O: while a song plays the game opens
    // no maps\...\_pc.ipk bundles (all preloaded), so "no game-file open for a while +
    // the game window up front" == in a song. This never mutes keys -- at most it swaps
    // dpadArmDwellMs -> dpadArmDwellGameplayMs so the clutch is firmer to wake (ESC guard).
    // OFF by default: the "file quiet for N s == in a song" heuristic has never been
    // confirmed to fire on the shipping build, so it is opt-in (suppress_in_game = 1)
    // until a better gameplay signal exists. The gameprobe CreateFile hooks still run
    // (they stamp g_lastOpenTick) so that work can be done later. With this false,
    // dpadArmDwellGameplayMs / fileIdleGameplayMs are dormant knobs.
    bool   suppressInGame     = false;
    int    fileIdleGameplayMs = 9000;   // no game-file open for this long (+ window up front) => in a song

    int    armAfterFrames    = 12;      // ignore this many frames after (re)acquiring a hand while the filter settles

    // Signal conditioning for the dominant hand (air d-pad): a 1 Euro filter -> position.
    float  filter1eMinCutoff = 1.0f;   // 1e f_cmin (Hz): low => smoother at rest
    float  filter1eBeta      = 0.05f;  // 1e speed coefficient: high => less lag when fast
    float  filter1eDCutoff   = 1.0f;   // 1e derivative cutoff (Hz)

    // output virtual-keys
    unsigned keyLeft         = 0x25;    // VK_LEFT
    unsigned keyRight        = 0x27;    // VK_RIGHT
    unsigned keyUp           = 0x26;    // VK_UP
    unsigned keyDown         = 0x28;    // VK_DOWN
    unsigned keyConfirm      = 0x0D;    // VK_RETURN
    unsigned keyBack         = 0x1B;    // VK_ESCAPE
    int    keyPressMs        = 40;

    bool   trace             = false;   // ~15 Hz hand-signal trace to the log
    bool   overlay           = false;   // on-screen HUD window -- OFF by default (windowed
                                        // mode only; opt in with overlay = 1 for troubleshooting)
};

namespace Cfg
{
    const Config& Load();   // kinectnav.ini over the defaults. Idempotent.
    const Config& Get();
}
