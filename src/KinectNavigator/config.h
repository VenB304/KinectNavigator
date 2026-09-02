#pragma once
#include "framework.h"

// Config. Hardcoded defaults, overridable by a "kinectnav.ini" next to the
// module (key = value, '#' or ';' comments -- see dist/kinectnav.example.ini).
// The .ini key names are the snake_case of these fields (see config.cpp).
//
// v0.8 -- SWIPE model. No engage state. Distances are torso units
// (torso = |SHOULDER_CENTER - HIP_CENTER|, ~0.41 m); speeds torso/second; times
// in ms of the sensor frame-clock.
//
//   Left/Right   : horizontal hand swipe. Repeat: swipe, then hold the arm
//                  fully EXTENDED -- accelerating auto-repeat until it relaxes.
//   Up/Down      : vertical hand swipe. One step, no repeat.
//   Confirm(Ent) : dominant hand raised + still, held confirmDwellMs.
//   Back  (Esc)  : NON-dominant arm pointing down-and-out to the side (~45 deg),
//                  held backDwellMs.

struct Config
{
    bool   enable            = true;
    bool   mirror            = false;   // flip left/right output
    bool   leftHanded        = false;   // dominant hand = LEFT
    bool   enableConfirm     = true;
    bool   enableBack        = true;
    bool   requireForeground = true;    // only emit keys while the game window is foreground

    // which recogniser runs. 1 = "extend": a shoulder-centred air d-pad (postural).
    // 0 = "swipe": the layered v0.8 motion model (kept as a fallback -- nav_model = swipe).
    int    navModel          = 1;

    // --- nav_model = extend : air d-pad centred on the dominant shoulder (frontal plane,
    // torso units; +x = outward on the dominant side after `mirror`, +y = up) ---
    // Clutch: starts DISARMED. Park the hand at the shoulder (dpadArmDwellMs) to arm it.
    // It auto-disarms after the hand sits in no-man's-land (not parked, not in a wedge --
    // arm hanging / dancing / gesturing) for dpadDisarmMs. Re-arm = park again.
    bool   dpadArm           = true;    // 0 = always live (no clutch)
    int    dpadArmDwellMs    = 200;     // hold the hand in the park box this long to arm
    int    dpadDisarmMs      = 1500;    // ...hand idle out of park & out of every wedge this long -> disarm

    float  dpadParkRadius    = 0.32f;   // |hand - dom shoulder| under this = parked / home / off
    float  dpadParkExitK     = 1.35f;   // must reach parkRadius * this to LEAVE park (hysteresis)
    float  dpadWedgeHalfDeg  = 24.0f;   // half-angle of the R/L/U wedges; the gaps between are dead
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
    int    dpadBackDwellMs   = 3000;    // ...but hold down/left this long before ESC fires (Esc is
                                        // consequential -- pause/exit -- so it wants a firm hold)
    int    dpadCmdRepeatMs   = 700;     // command-mode (Enter/Esc) slow repeat interval

    // gameplay mute via the game's file I/O: a song bundle load-burst then file
    // quiet == in a song. Un-mutes on the next game-file open (back in a menu).
    bool   suppressInGame     = true;
    int    songBurstCount     = 6;      // consecutive opens of the SAME _pc.ipk (nothing
                                        // else between) within songBurstMs -- a real song
                                        // load; coach-select reopens are interspersed
    int    songBurstMs        = 1500;   // a real "press play" hammers the bundle sub-second;
                                        // a lazy song-list hover-preview is more spread out
    int    fileIdleGameplayMs = 9000;   // ...then file-quiet this long => in the song
    int    gameplayCapMs      = 360000; // failsafe un-mute

    // swipe detector (dominant hand for nav; body-relative to SHOULDER_CENTER).
    // Runs on the CLEAN signal: 1 Euro filtered position, Savitzky-Golay velocity
    // (Stage 1). SG peaks run ~30-50% below the old EMA central-diff, hence the
    // lower swipeVelocity.
    float  swipeVelocity     = 1.0f;    // min hand speed (torso/s) to be swiping
    float  swipeVxCeiling    = 16.0f;   // loose sanity clamp -- SG already kills single-frame spikes
    float  swipePeakRatio    = 1.9f;    // to fire, peak speed during the swipe must reach this *
                                        // swipeVelocity (=1.9 torso/s) -- rejects a slow constant-
                                        // velocity drift (an arm lowering to rest plateaus ~1.4 on
                                        // the SG stream; a real ballistic stroke peaks 3-5). 1.0 off.
    float  swipeDistance     = 0.28f;   // net travel in the swipe direction to fire
    int    swipeMinFrames    = 2;       // frames of sustained one-way motion (in the current segment)
                                        // before it fires
    // The swipe is segmented at direction reversals: a counter-flick ("countersteer") then the real
    // stroke are two segments, and only the LAST segment is judged -- measured from the point the
    // hand turned around. So an initial opposite flick is absorbed no matter its size or speed.
    int    swipeMaxSegments  = 4;       // more direction changes than this => fiddling, not a swipe
    int    swipeMaxActiveFrames = 24;   // a swipe that hasn't fired within this many frames aborts
    float  swipeAxisRatio    = 1.4f;    // dominant-axis speed must beat the other axis by this
    int    swipeCooldownMs   = 500;     // after a swipe fires: no new swipe for this long
    int    swipeSettleAfterMs = 1200;   // ...and for this long the hand must also come to rest
                                        // (settle gate) before the next swipe -- enforced ONLY in
                                        // this post-swipe window, so a cold swipe from rest is never
                                        // gated by inferred-hand velocity noise
    int    swipeSettleFrames = 3;       // settle = hand slow this many (leaky-counted) frames
    float  swipeSettleFrac   = 0.7f;    // "slow" = both speeds under this * swipeVelocity
    float  swipeYMin         = -0.95f;  // a vertical swipe only fires while the hand is in the play
    float  swipeYMax         =  1.10f;  // zone (torso, from SHOULDER_CENTER) -- an arm dropping to
                                        // rest passes below swipeYMin so it's not a DOWN
    float  swipeVertXBand    =  0.55f;  // vertical swipes only when the hand is roughly in FRONT of
                                        // the body (|ex| under this) -- a hand recovering out to the
                                        // side toward rest won't register as Up/Down

    // --- positional invariants (Stage 2): the stroke must END in the command's target
    // zone. A recovery / return-to-park stroke ends near neutral and is rejected with no
    // latency, no state. All numbers torso units, from SHOULDER_CENTER; +y up, +x = user's
    // right. Provisional (Gemini code review) -- tune on the replay corpus + in-game.
    float  upEndMinY         =  0.15f;  // a fired UP must end with the hand at least this high
    float  upElbowMargin     =  0.05f;  // ...and hand.y >= elbow.y - this (a real forearm raise, not
                                        // a recovery drag where the hand trails below the elbow).
                                        // Skipped when the elbow isn't tracked. 9 = disable.
    float  downStartMinY     = -0.45f;  // a fired DOWN's segment must START at/above this (not a
                                        // twitch from an already-hanging arm) -- kept lenient;
                                        // the END gate below is the real discriminator
    float  downEndMaxY       = -0.40f;  // ...and END at/below this (a recovery-DOWN ends near neutral)
    float  leftEndMaxX       = -0.10f;  // a fired LEFT (dominant hand) must cross the midline to here
    float  rightEndMinX      =  0.35f;  // a fired RIGHT must reach outward to here

    // Left/Right hold-to-repeat: after an L/R swipe, extend the arm straight OUT TO THE SIDE
    int    postSwipeMs       = 400;     // window after an L/R swipe to catch the extend
    float  armExtendFrac     = 0.90f;   // |hand-shoulder| / (shoulder-elbow + elbow-hand) above this = extended
    float  armRelaxFrac      = 0.80f;   // ...drops below this => stop repeating
    float  armLevelTanMax    = 1.0f;    // repeat only while the arm is within this |dy|/|dx| of
                                        // horizontal (1.0 = +-45 deg) -- a straight arm held high or
                                        // hanging low no longer counts. Exit uses 1.4x this.
    int    repeatFirstMs     = 430;     // first auto-repeat interval
    int    repeatMinMs       = 200;     // fastest it accelerates to
    int    repeatAccelMs     = 22;      // each repeat shortens the interval by this

    // Confirm -- dominant hand raised + roughly still
    int    confirmDwellMs    = 1800;
    int    confirmRepeatMs   = 550;     // once repeating, fire this often while the hand stays raised
                                        // (hold to page through a menu). 0 = one-shot, no repeat.
    int    confirmRepeatFirstMs = 900;  // ...but wait this long after the FIRST Confirm before the
                                        // first repeat -- grace to lower the hand after a single one
    float  confirmRaiseFy    = 0.25f;   // hand.y - SHOULDER_CENTER.y above this (torso) = "raised".
                                        // Was 0.05 (barely above the shoulder) -- with the SG stream
                                        // now reliably detecting "still", a casual raise was
                                        // completing the dwell. 0.25 = a deliberate high hold.
    float  confirmStillVel   = 0.50f;   // and |hand velocity| under this (torso/s). On the SG stream
                                        // a held hand sits well under 0.3 -- the old 1.2 (for the
                                        // jittery EMA) let almost any raised hand count as "still".

    // Back -- NON-dominant arm pointing down-and-out to the side (~45 deg)
    int    backDwellMs       = 1800;
    int    backRepeatMs      = 550;     // once repeating, fire this often while the pose is held
                                        // (hold to back all the way out). 0 = one-shot, no repeat.
    int    backRepeatFirstMs = 900;     // ...grace after the first Back before the first repeat
    float  backOutMin        = 0.30f;   // unit(hand - shoulder): sideways-away component at least this
    float  backDownMin       = 0.22f;   // ...and downward component at least this
    float  backDownMax       = 0.88f;   // ...but not straight down (that is just a resting arm)
    float  backStillVel      = 1.2f;    // |hand velocity| under this (torso/s)

    int    dwellGraceFrames  = 20;      // Confirm/Back: consecutive bad frames tolerated (~0.66 s) before reset
    int    armAfterFrames    = 12;      // ignore this many frames after (re)acquiring a hand while smoothing settles

    // exponential smoothing -- NON-dominant hand only now (Back pose speed gate).
    // The dominant hand / swipe path runs on the 1 Euro + Savitzky-Golay signal below.
    float  smoothFast        = 0.5f;    // fast track (velocity)
    float  smoothSlow        = 0.15f;   // slow track (held pose)

    // Signal conditioning for the dominant hand / swipe path (Stage 1): a 1 Euro
    // filter -> position, a 5-point Savitzky-Golay first derivative -> velocity.
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
