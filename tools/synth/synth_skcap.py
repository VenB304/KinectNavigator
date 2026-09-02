#!/usr/bin/env python3
"""Generate synthetic .skcap captures for KinectNavigator recognizer testing.

A synthetic capture has a fixed, fully-tracked skeleton (torso anchor rock
steady) with only HAND_RIGHT / HAND_LEFT driven along a scripted path. Feed the
files through KinectNavigatorReplay to check the recognizer deterministically -- no
Kinect, no recording session.

    python synth_skcap.py            # write all cases into ./out/
    python synth_skcap.py --run <path-to-KinectNavigatorReplay.exe>   # + replay & check

Frame format matches src/KinectNavigator/nui_types.h (pack(8)):
  NUI_SKELETON_FRAME = 2664 bytes, NUI_SKELETON_DATA = 436, Vector4 = 16.
Header matches Recorder::Header (pack(1), 24 bytes): "SKCAP01\n", frameSize=2664.
"""

import argparse
import os
import struct
import subprocess
import sys

FPS = 30
DT_MS = 1000.0 / FPS
TORSO = 0.45  # metres, SC -> HIP

# NUI joint indices
HIP_CENTER, SPINE, SHOULDER_CENTER, HEAD = 0, 1, 2, 3
SHOULDER_LEFT, ELBOW_LEFT, WRIST_LEFT, HAND_LEFT = 4, 5, 6, 7
SHOULDER_RIGHT, ELBOW_RIGHT, WRIST_RIGHT, HAND_RIGHT = 8, 9, 10, 11
HIP_LEFT, KNEE_LEFT, ANKLE_LEFT, FOOT_LEFT = 12, 13, 14, 15
HIP_RIGHT, KNEE_RIGHT, ANKLE_RIGHT, FOOT_RIGHT = 16, 17, 18, 19
NJOINTS = 20

TRACKED = 2  # joint + skeleton tracked

# Base pose in metres. +X = user's right (established from real captures),
# +Y = up, +Z = away toward the room. SC at the origin of the vertical axis.
Z = 2.60
BASE = {
    HIP_CENTER:      (0.00, -0.45, Z),
    SPINE:           (0.00, -0.22, Z),
    SHOULDER_CENTER: (0.00,  0.00, Z),
    HEAD:            (0.00,  0.28, Z),
    SHOULDER_LEFT:   (-0.18, -0.02, Z),
    SHOULDER_RIGHT:  (0.18, -0.02, Z),
    ELBOW_LEFT:      (-0.22, -0.40, Z),
    ELBOW_RIGHT:     (0.22, -0.40, Z),
    WRIST_LEFT:      (-0.21, -0.72, Z - 0.05),
    WRIST_RIGHT:     (0.21, -0.72, Z - 0.05),
    HAND_LEFT:       (-0.20, -0.80, Z - 0.05),
    HAND_RIGHT:      (0.20, -0.80, Z - 0.05),
    HIP_LEFT:        (-0.12, -0.46, Z),
    HIP_RIGHT:       (0.12, -0.46, Z),
    KNEE_LEFT:       (-0.13, -0.95, Z),
    KNEE_RIGHT:      (0.13, -0.95, Z),
    ANKLE_LEFT:      (-0.13, -1.35, Z),
    ANKLE_RIGHT:     (0.13, -1.35, Z),
    FOOT_LEFT:       (-0.13, -1.42, Z + 0.10),
    FOOT_RIGHT:      (0.13, -1.42, Z + 0.10),
}

# Named hand targets (metres, absolute), for readable scripts.  +x = user's right, +y = up.
REST_R   = (0.20, -0.80, Z - 0.05)   # right arm hanging
REST_L   = (-0.20, -0.80, Z - 0.05)  # left arm hanging
MID_R    = (0.05, -0.05, Z - 0.10)   # right hand at chest centre (swipe start/return)
FAR_R    = (0.60, -0.05, Z - 0.10)   # right hand out to the user's right
FAR_L    = (-0.55, -0.05, Z - 0.10)  # right hand out to the user's left
HI_R     = (0.05,  0.42, Z - 0.10)   # right hand up
LO_R     = (0.05, -0.45, Z - 0.10)   # right hand down (but above the hanging rest)
RAISE_R  = (0.05,  0.22, Z - 0.10)   # right hand gently raised + held (Confirm dwell)
EXT_R    = (0.78, -0.05, Z - 0.30)   # right hand far out + forward: arm near full extension
BACK_L   = (-0.55, -0.48, Z + 0.02)  # left hand: down-and-out to the side ~45deg (Back pose)


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def build_track(keys):
    """keys: list of (t_seconds, hand_r_xyz, hand_l_xyz). Linear-interp at FPS.
    Returns list of (hand_r, hand_l) per frame."""
    total_s = keys[-1][0]
    n = int(round(total_s * FPS)) + 1
    frames = []
    for i in range(n):
        t = i / FPS
        # find bracketing keys
        k0 = keys[0]
        k1 = keys[-1]
        for j in range(len(keys) - 1):
            if keys[j][0] <= t <= keys[j + 1][0]:
                k0, k1 = keys[j], keys[j + 1]
                break
        span = max(k1[0] - k0[0], 1e-6)
        u = (t - k0[0]) / span
        u = max(0.0, min(1.0, u))
        hr = lerp(k0[1], k1[1], u)
        hl = lerp(k0[2], k1[2], u)
        frames.append((hr, hl))
    return frames


def pack_skeleton_data(hand_r, hand_l, tracked=True, wobble=None):
    pos = dict(BASE)
    if wobble:  # (frame_index) -> add whole-body dance-like motion
        import math
        i = wobble
        sway = 0.12 * math.sin(i * 0.5)
        bob = 0.09 * math.sin(i * 0.9 + 1.0)
        for j in pos:
            x, y, z = pos[j]
            pos[j] = (x + sway + 0.03 * math.sin(i * 1.7 + j),
                      y + bob + 0.03 * math.cos(i * 1.5 + j),
                      z + 0.04 * math.sin(i * 0.7 + j))
        hand_r = (hand_r[0] + sway + 0.03 * math.sin(i * 2.1),
                  hand_r[1] + bob + 0.03 * math.cos(i * 1.9), hand_r[2])
        hand_l = (hand_l[0] + sway - 0.03 * math.sin(i * 2.0),
                  hand_l[1] + bob + 0.03 * math.cos(i * 2.2), hand_l[2])
    pos[HAND_RIGHT] = hand_r
    pos[WRIST_RIGHT] = (hand_r[0], hand_r[1] + 0.05, hand_r[2])
    pos[HAND_LEFT] = hand_l
    pos[WRIST_LEFT] = (hand_l[0], hand_l[1] + 0.05, hand_l[2])

    buf = bytearray()
    buf += struct.pack("<I", TRACKED if tracked else 0)   # eTrackingState
    buf += struct.pack("<I", 1)                            # dwTrackingID
    buf += struct.pack("<I", 0)                            # dwEnrollmentIndex
    buf += struct.pack("<I", 0)                            # dwUserIndex
    buf += struct.pack("<4f", 0.0, 0.0, Z, 1.0)            # Position
    for j in range(NJOINTS):
        x, y, z = pos[j]
        buf += struct.pack("<4f", x, y, z, 1.0)
    for j in range(NJOINTS):
        buf += struct.pack("<I", TRACKED)                  # eSkeletonPositionTrackingState
    buf += struct.pack("<I", 0)                            # dwQualityFlags
    assert len(buf) == 436, len(buf)
    return bytes(buf)


EMPTY_SKELETON = None


def empty_skeleton():
    global EMPTY_SKELETON
    if EMPTY_SKELETON is None:
        buf = bytearray(436)
        # eTrackingState = 0 (NOT_TRACKED), everything else zero
        EMPTY_SKELETON = bytes(buf)
    return EMPTY_SKELETON


def write_skcap(path, track, dance=False):
    with open(path, "wb") as f:
        f.write(struct.pack("<8sIIQ", b"SKCAP01\n", 2664, 0, 0))
        for i, (hr, hl) in enumerate(track):
            ts_ms = int(round(i * DT_MS))
            frame = bytearray()
            frame += struct.pack("<q", ts_ms)             # liTimeStamp (ms)
            frame += struct.pack("<I", 1000 + i)          # dwFrameNumber
            frame += struct.pack("<I", 0)                 # dwFlags
            frame += struct.pack("<4f", 0, -1, 0, 1.6)    # vFloorClipPlane
            frame += struct.pack("<4f", 0, 1, 0, 0)       # vNormalToGravity
            frame += pack_skeleton_data(hr, hl, wobble=(i if dance else None))  # SkeletonData[0]
            for _ in range(5):                            # SkeletonData[1..5]
                frame += empty_skeleton()
            assert len(frame) == 2664, len(frame)
            f.write(frame)


# ---- test cases (v0.8 swipe model) --------------------------------------
# Each: (name, keyframes, description[, dance]).  keyframe = (t_seconds, hand_r, hand_l)

CASES = []

CASES.append((
    "idle",
    [(0.0, REST_R, REST_L), (6.0, REST_R, REST_L)],
    "arms down 6 s -> nothing",
))

def swipe_case(name, a, b, act):
    # rest -> SLOW setup to A -> settle -> FAST swipe A->B -> hold -> SLOW return -> rest
    return (name, [
        (0.0, REST_R, REST_L),
        (2.5, a, REST_L),           # slow setup (well below swipe speed)
        (3.2, a, REST_L),           # settle
        (3.33, b, REST_L),          # FAST ~0.13 s swipe
        (3.8, b, REST_L),           # hold out
        (6.0, a, REST_L),           # slow return (below swipe speed)
        (8.0, REST_R, REST_L),      # slow drop to rest
    ], f"fast swipe -> one {act}, no repeat")

CASES.append(swipe_case("swipe_right", MID_R, FAR_R, "RIGHT"))
CASES.append(swipe_case("swipe_left",  MID_R, FAR_L, "LEFT"))
CASES.append(swipe_case("swipe_up",    LO_R,  HI_R,  "UP"))
CASES.append(swipe_case("swipe_down",  HI_R,  LO_R,  "DOWN"))

CASES.append((
    "confirm_hold",
    [(0.0, REST_R, REST_L),
     (2.3, RAISE_R, REST_L),        # slow raise (well below swipe speed)
     (5.0, RAISE_R, REST_L),        # hold ~2.7 s (repeat disabled for this case -> one Confirm)
     (7.5, REST_R, REST_L)],        # slow drop (>2 s so it isn't a DOWN swipe)
    "raise dominant hand + hold still 2 s -> Confirm",
))

CASES.append((
    "confirm_repeat",
    [(0.0, REST_R, REST_L),
     (2.3, RAISE_R, REST_L),        # raise and KEEP holding
     (8.0, RAISE_R, REST_L),        # ~5.7 s held -> first Confirm then auto-repeats
     (10.2, REST_R, REST_L)],       # slow drop
    "hold the raise -> Confirm then auto-repeats",
))

CASES.append((
    "back_pose",
    [(0.0, REST_R, REST_L),
     (1.4, REST_R, BACK_L),         # left arm out-and-down to ~45 deg
     (3.6, REST_R, BACK_L),         # hold ~2.2 s -> one Back
     (4.4, REST_R, REST_L)],
    "non-dominant arm down-and-out ~45 deg, held 2 s -> Back",
))

CASES.append((
    "back_repeat",
    [(0.0, REST_R, REST_L),
     (1.4, REST_R, BACK_L),
     (7.0, REST_R, BACK_L),         # ~5.6 s held -> first Back then auto-repeats
     (7.8, REST_R, REST_L)],
    "hold the Back pose -> Back then auto-repeats",
))

WINDUP_L = (-0.09, -0.05, Z - 0.10)   # quick counter-flick left (~0.28 torso), before a right swipe

CASES.append((
    "windup_swipe_right",   # counter-flick left ("countersteer"), then full swipe right -> one RIGHT
    [(0.0, REST_R, REST_L),
     (2.5, MID_R, REST_L), (3.2, MID_R, REST_L),     # slow setup + settle
     (3.33, WINDUP_L, REST_L),                       # ~0.13 s fast counter-flick left
     (3.50, FAR_R, REST_L), (3.9, FAR_R, REST_L),    # FAST swipe right + hold
     (6.0, MID_R, REST_L), (8.0, REST_R, REST_L)],
    "counter-flick left then full swipe right -> one RIGHT",
))

REST_OUT_R = (0.55, -0.05, Z - 0.10)   # hand parked out to the right (no recent fire)
IN_NEAR    = (0.02, -0.05, Z - 0.10)   # waved back in toward the torso centre

CASES.append((
    "waveback_right_fast",   # hand parked right -> fast wave IN toward torso -> fast back OUT right
    [(0.0, REST_R, REST_L),
     (2.5, REST_OUT_R, REST_L), (3.4, REST_OUT_R, REST_L),   # park out right + settle (no recent fire)
     (3.70, IN_NEAR, REST_L),                                # ~0.30 s wave in toward torso (~0.53 m)
     (4.05, REST_OUT_R, REST_L), (4.6, REST_OUT_R, REST_L),  # ~0.35 s back out to the right + hold
     (6.5, REST_R, REST_L)],
    "parked right, fast wave in to torso then back out -> want RIGHT (or nothing), NOT a premature LEFT",
))

CASES.append((
    "waveback_right_slow",   # same but a lazier wave in
    [(0.0, REST_R, REST_L),
     (2.5, REST_OUT_R, REST_L), (3.4, REST_OUT_R, REST_L),
     (4.05, IN_NEAR, REST_L),                                # ~0.65 s wave in (~1.4 torso/s)
     (4.75, REST_OUT_R, REST_L), (5.3, REST_OUT_R, REST_L),
     (7.0, REST_R, REST_L)],
    "parked right, lazy wave in to torso then back out -> want RIGHT (or nothing), NOT LEFT",
))

NEUTRAL_R = (0.05, -0.05, Z - 0.10)   # right hand at chest-centre height (scroll neutral)

CASES.append((
    "scroll_down_x3",   # three down-swipes with a recovery raise back to neutral between each
    [(0.0, REST_R, REST_L),
     (2.3, NEUTRAL_R, REST_L), (3.0, NEUTRAL_R, REST_L),     # settle at neutral
     (3.15, LO_R, REST_L),                                   # DOWN #1 (fast)
     (3.6, NEUTRAL_R, REST_L), (4.1, NEUTRAL_R, REST_L),     # recovery raise to neutral + settle
     (4.25, LO_R, REST_L),                                   # DOWN #2
     (4.7, NEUTRAL_R, REST_L), (5.2, NEUTRAL_R, REST_L),
     (5.35, LO_R, REST_L),                                   # DOWN #3
     (5.8, NEUTRAL_R, REST_L), (7.0, REST_R, REST_L)],
    "down, recover to neutral, down, recover, down -> 3 DOWN and ZERO UP (recovery never reaches the up-zone)",
))

CASES.append((
    "double_swipe_right",   # two clean swipes with a settle between -> two RIGHT
    [(0.0, REST_R, REST_L),
     (2.5, MID_R, REST_L), (3.2, MID_R, REST_L),
     (3.33, FAR_R, REST_L), (3.7, FAR_R, REST_L),
     (4.6, MID_R, REST_L), (5.3, MID_R, REST_L),     # return + settle
     (5.43, FAR_R, REST_L), (5.8, FAR_R, REST_L),
     (7.0, MID_R, REST_L), (9.0, REST_R, REST_L)],
    "swipe, settle, swipe again -> RIGHT x2",
))

# ---- nav_model = extend : air d-pad (per-case ini pins nav_model = extend) ----
DP_PARK  = (0.20, -0.02, Z - 0.05)   # right hand at the right shoulder = parked / home
DP_RIGHT = (0.90, -0.02, Z - 0.10)   # arm out to the user's right
DP_UP    = (0.22,  0.55, Z - 0.10)   # arm up
DP_DOWNO = (0.55, -0.80, Z - 0.10)   # arm down-and-out (below + outward -> DOWN wedge)
DP_LSHLD = (-0.18, -0.02, Z)         # left hand on the left shoulder = command mode

CASES.append(("dpad_idle_rest",
    [(0.0, REST_R, REST_L), (6.0, REST_R, REST_L)],
    "d-pad: both arms hanging at rest -> NOTHING (never parks -> clutch never arms)"))
CASES.append(("dpad_park_noop",
    [(0.0, REST_R, REST_L), (1.0, DP_PARK, REST_L), (5.0, DP_PARK, REST_L), (6.0, REST_R, REST_L)],
    "d-pad: right hand parked at the shoulder (arms the clutch) -> still NOTHING"))
CASES.append(("dpad_right",
    [(0.0, REST_R, REST_L), (1.0, DP_PARK, REST_L), (1.9, DP_PARK, REST_L),   # park -> arm
     (2.2, DP_RIGHT, REST_L), (5.2, DP_RIGHT, REST_L), (5.7, DP_PARK, REST_L)],
    "d-pad: park to arm, reach right, hold -> RIGHT then auto-repeat"))
CASES.append(("dpad_up",
    [(0.0, REST_R, REST_L), (1.0, DP_PARK, REST_L), (1.9, DP_PARK, REST_L),
     (2.2, DP_UP, REST_L), (5.2, DP_UP, REST_L), (5.7, DP_PARK, REST_L)],
    "d-pad: park to arm, reach up, hold -> UP then auto-repeat"))
CASES.append(("dpad_down",
    [(0.0, REST_R, REST_L), (1.0, DP_PARK, REST_L), (1.9, DP_PARK, REST_L),
     (2.2, DP_DOWNO, REST_L), (5.2, DP_DOWNO, REST_L), (5.7, DP_PARK, REST_L)],
    "d-pad: park to arm, reach down-and-out, hold -> DOWN then auto-repeat"))
CASES.append(("dpad_confirm",
    [(0.0, REST_R, REST_L), (1.0, DP_PARK, DP_LSHLD), (1.9, DP_PARK, DP_LSHLD),
     (2.2, DP_UP, DP_LSHLD), (5.2, DP_UP, DP_LSHLD), (5.7, DP_PARK, REST_L)],
    "d-pad: park to arm, left hand on shoulder + reach up -> CONFIRM then slow repeat"))
CASES.append(("dpad_back",
    [(0.0, REST_R, REST_L), (1.0, DP_PARK, DP_LSHLD), (1.9, DP_PARK, DP_LSHLD),
     (2.2, DP_DOWNO, DP_LSHLD), (7.5, DP_DOWNO, DP_LSHLD), (8.0, DP_PARK, REST_L)],
    "d-pad: park to arm, left hand on shoulder + reach down-and-out, hold 3 s -> BACK"))

# expected replay tallies per case. missing key => expect 0. tuple => (lo, hi) inclusive.
EXPECT = {
    "idle":               {},
    "swipe_right":         {"R": 1},
    "swipe_left":          {"L": 1},
    "swipe_up":            {"U": 1},
    "swipe_down":          {"D": 1},
    "confirm_hold":        {"Confirm": 1},
    "confirm_repeat":      {"Confirm": (3, 9)},   # first + several auto-repeats over ~5.7 s
    "back_pose":           {"Back": 1},
    "back_repeat":         {"Back": (3, 9)},
    "windup_swipe_right":  {"R": 1},
    # Wave-in -> back-out: the Stage-2 midline invariant (a LEFT must end past ex = left_end_max_x)
    # rejects both, since a wave-in stops near centre. Resolves to RIGHT on the way back, or nothing.
    "waveback_right_fast": {"R": (0, 1)},
    "waveback_right_slow": {"R": (0, 1)},
    "scroll_down_x3":      {"D": (2, 3)},   # key assertion: U == 0 (recovery raises never fire UP)
    "double_swipe_right":  {"R": 2},
    # --- nav_model = extend ---
    "dpad_idle_rest":      {},
    "dpad_park_noop":      {},
    "dpad_right":          {"R": (1, 14)},
    "dpad_up":             {"U": (1, 14)},
    "dpad_down":           {"D": (1, 14)},
    "dpad_confirm":        {"Confirm": (1, 6)},
    "dpad_back":           {"Back": (1, 6)},
}
TALLY_KEYS = ["L", "R", "U", "D", "Confirm", "Back"]

# base ini for the swipe-model cases; the d-pad cases override nav_model.
BASE_INI = "enable_back = 1\nnav_model = swipe\n"
CASE_INI = {
    "confirm_hold": "confirm_repeat_ms = 0\n",
    "back_pose":    "back_repeat_ms = 0\n",
    "dpad_idle_rest": "nav_model = extend\n",
    "dpad_park_noop": "nav_model = extend\n",
    "dpad_right":     "nav_model = extend\n",
    "dpad_up":        "nav_model = extend\n",
    "dpad_down":      "nav_model = extend\n",
    "dpad_confirm":   "nav_model = extend\n",
    "dpad_back":      "nav_model = extend\n",
}


def check(name, got):
    exp = EXPECT.get(name, {})
    bad = []
    for k in TALLY_KEYS:
        want = exp.get(k, 0)
        g = got.get(k, 0)
        if isinstance(want, tuple):
            if not (want[0] <= g <= want[1]):
                bad.append(f"{k}={g} want {want[0]}-{want[1]}")
        elif g != want:
            bad.append(f"{k}={g} want {want}")
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "out"))
    ap.add_argument("--run", metavar="REPLAY_EXE",
                    help="path to KinectNavigatorReplay.exe; replay each case and print a summary")
    args = ap.parse_args()

    # the Back gesture ships disabled; the regression still exercises its detector
    ini = os.path.join(os.path.dirname(args.run), "kinectnav.ini") if args.run else None
    if ini:
        try:
            with open(ini, "w") as fh:
                fh.write(BASE_INI)
        except OSError:
            ini = None

    os.makedirs(args.out, exist_ok=True)
    made = []
    for case in CASES:
        name, keys, desc = case[0], case[1], case[2]
        dance = len(case) > 3 and case[3]
        track = build_track(keys)
        path = os.path.join(args.out, f"{name}.skcap")
        write_skcap(path, track, dance=dance)
        made.append((name, path, desc, len(track)))
        print(f"  {name:24s} {len(track):4d} frames  ({keys[-1][0]:.1f}s)   {desc}")

    if not args.run:
        print(f"\n{len(made)} files in {args.out}")
        print("replay one:  KinectNavigatorReplay.exe out\\<name>.skcap --step")
        return

    replay = args.run
    if not os.path.isfile(replay):
        sys.exit(f"not found: {replay}")
    print(f"\n--- replaying through {replay} ---")
    npass = 0
    for name, path, desc, _ in made:
        if ini:
            try:
                with open(ini, "w") as fh:
                    fh.write(BASE_INI + CASE_INI.get(name, ""))
            except OSError:
                pass
        r = subprocess.run([replay, path, "--step"], capture_output=True, text=True)
        out = r.stdout + r.stderr
        keys_ = {
            "ENGAGED": out.count(": ENGAGED"),
            "DISENGAGED": out.count("DISENGAGED"),
            "L": out.count("VK_LEFT"), "R": out.count("VK_RIGHT"),
            "U": out.count("VK_UP"), "D": out.count("VK_DOWN"),
            "Confirm": out.count("FIRE Confirm") + out.count("dpad CONFIRM"),
            "Back":    out.count("FIRE Back")    + out.count("dpad BACK"),
        }
        summ = " ".join(f"{k}={v}" for k, v in keys_.items() if v) or "(nothing)"
        bad = check(name, keys_)
        if bad:
            print(f"  FAIL  {name:24s} {summ}")
            print(f"        {'':24s} -> {'; '.join(bad)}")
        else:
            npass += 1
            print(f"  ok    {name:24s} {summ}")
    print(f"\n{npass}/{len(made)} passed")
    sys.exit(0 if npass == len(made) else 1)


if __name__ == "__main__":
    main()
