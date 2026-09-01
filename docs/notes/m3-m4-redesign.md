# M3 / M4 redesign — post-research draft

Status: **draft for review.** Not yet folded into `docs/build-plan.html` (that becomes plan
v0.7 once the direction is signed off). Input: `docs/research/Kinect Gesture Navigation UI Design.md`
(Gemini Deep Research) + `docs/notes/research-thresholds.md`.

## Why the model changes

The research and our own live tests agree on the core problem: **holding an arm out is
tiring** (Test 2), and a swipe-per-item is exhausting (Test 1). The research's answer is to
stop treating navigation as a *motion* (swipe) and treat it as a *position* — hand parked in
a body-anchored zone — with an explicit engage/disengage state to kill false fires, and to
reuse the swipe only for the rare, deliberate Confirm/Back.

This is a navigation-model change, not a tuning change. It redefines M3 and absorbs the
Confirm/Back/either-hand parts of M4.

### What stays

- Middle-layer `Kinect10.dll` proxy, keystroke output, menu-nav-only scope — unchanged.
- The pipeline: tap → `framebuffer` → `recognizer` thread picks navigator → `gestures` →
  `output`. All still valid.
- Navigator pick (closest + centred, sticky by tracking ID), torso-unit normalisation,
  dual-alpha smoothing, timing off `liTimeStamp` — all keep.
- `SkeletonKeyReplay` corpus + `SkeletonKeyLab` (new) are how we tune it.

### What changes

| | Now (build `b419dd2`) | After redesign |
|---|---|---|
| Navigation trigger | right-hand **swipe** (velocity + distance) | hand **position** vs. a dead-zone / activation band around a body-anchored origin |
| Engage state | none (wave re-arm is the nearest thing) | explicit: dominant hand raised to chest-centre, held ~600 ms |
| Disengage | body lost / re-arm timeout | dominant hand drops below hip-centre — instant, forgiving (asymmetric hysteresis) |
| Up / Down | not implemented | same positional model on Y |
| Long lists | "swipe once / hold hand out to auto-repeat" | hand held past activation → rate-controlled auto-repeat (~3 Hz) — "virtual joystick" |
| Confirm | unimplemented (was: dwell) | ballistic cross-body swipe, **dominant** hand, > 1.2 m/s, > 0.3 m |
| Back | unimplemented (was: hands-on-hips pose) | ballistic cross-body swipe, **non-dominant** hand |
| Hands watched | right only | both (dominant = nav + confirm, non-dominant = back) |
| "Push to confirm" | dropped (bad Z) | stays dropped |
| Dwell-to-confirm | planned | dropped — research + Dance Central both reject it (fatigue, Midas-touch) |

## Revised M3 — "Positional navigator"

Goal: drive L/R/U/D menu movement hands-free, low-fatigue, no false fires at rest, verified
live in SkeletonKeyLab and in one short in-game confirmation.

1. **`gestures.cpp` rewrite — engage state machine.**
   `Disengaged → (dominant hand in engage zone, held engageDwellMs) → Engaged` — on entry,
   latch the smoothed dominant-hand position as `origin`. `Engaged → Disengaged` when the
   dominant hand drops below hip-centre (no dwell) or body lost.
   - Engage zone: `hand.y > SPINE.y`, `|hand.x - SC.x| < shoulderHalfWidth`, in torso units.
2. **Positional nav within Engaged.**
   Signed offset `d = (hand - origin) / torso`, per axis, slow-smoothed.
   - `|d| < deadZone` → neutral, emit nothing.
   - `|d|` crosses `activate` (with `activate > deadZone` hysteresis) → emit one arrow key
     for that axis/direction, then require return inside `deadZone` before the next discrete
     emit (or enter auto-repeat, below).
   - X and Y evaluated independently; if both are past `activate`, take the larger `|d|`
     (no diagonal emits).
3. **Auto-repeat ("virtual joystick").**
   Hand held past `activate` for `repeatDwellMs` → begin emitting that direction every
   `repeatIntervalMs` (~333). Stop when the hand returns inside `deadZone`. Optional gentle
   accel toward a floor interval (keep the existing `repeatAccelMs` idea, retune).
4. **Filtering.** Add explicit low-pass on the raw hand joint feeding the positional test —
   EMA now, 1€ filter if EMA lag/jitter trade-off is bad. Keep the existing fast/slow tracks
   for velocity vs. neutral. This is the make-or-break for the ±10/±15 cm bands vs. v1 jitter.
5. **Config.** New keys, all overridable by `kinectnav.ini`, defaults seeded from
   `research-thresholds.md` **as provisional** and converted to torso units:
   `engage_dwell_ms=600`, `disengage_below_hip=1`, `nav_dead_zone=0.25`,
   `nav_activate=0.375`, `repeat_dwell_ms=500`, `repeat_interval_ms=333`,
   `filter_alpha=…` (tune), `handedness=right`.
   Keep swipe_* keys — they move to Confirm/Back in M4.
6. **Tune offline** in `SkeletonKeyLab` (live sensor, no game) against fresh captures +
   capture 1's non-swipe blocks as a false-positive check. Then **one** short in-game pass to
   confirm arrow keys still scroll the carousel and the feel is acceptable.
7. **Open questions to settle during tuning**
   - Does `deadZone` / `activate` survive real inferred-hand jitter at ~2.75 m after
     filtering? If not: widen bands, or fall back to a coarser 3-zone (left / centre / right)
     scheme.
   - Is an explicit engage gesture acceptable UX, or annoying? Alternative: auto-engage when
     the dominant hand is raised and stable, no dwell.
   - Handedness: config default vs. auto-detect from which hand is raised at engage.

## Revised M4 — "Confirm / Back + polish"

1. **Confirm** — ballistic cross-body swipe, dominant hand, inward across the sternum.
   Reuse the existing swipe detector (velocity + distance + `swipeVxCeiling` jitter reject),
   retuned: `swipe_velocity ≈ 3.0 torso/s` (~1.2 m/s), `swipe_distance ≈ 0.75 torso`
   (~0.3 m), direction = inward. Only evaluated while Engaged. → `Enter`.
   - Risk carried over from Test 1: inferred-joint spikes fired keys before `swipeVxCeiling`.
     A velocity-gated confirm inherits that exact failure mode — watch for it.
2. **Back** — same ballistic swipe on the **non-dominant** hand. → `Esc`.
   Requires the non-dominant hand tracked + smoothed (this is the "either-hand" work, pulled
   forward from its old M4 slot).
3. **Disambiguation** — a fast nav correction must not read as a Confirm/Back swipe. Gate
   Confirm/Back on the hand *starting* near the body / inside the dead zone and moving
   inward, and on the global 500 ms cooldown.
4. **Audio** — per-action tick / distinct Confirm & Back stingers. The research flags this as
   near-essential given there's no cursor: the menu reacting is the only other feedback.
5. **Config to JSON?** Still optional. INI has been fine; revisit only if the key count gets
   unwieldy.

## Milestone tiles (for build-plan.html v0.7)

- **M3 — Positional navigator** *(in progress, redesigned)* — engage/disengage state +
  dead-zone/activation positional L/R/U/D + rate-controlled auto-repeat. Replaces the
  swipe-to-navigate model after the research pass. Tuned in SkeletonKeyLab.
- **M4 — Confirm / Back + either-hand + audio** — ballistic cross-body swipe: dominant →
  Enter, non-dominant → Esc. Per-action audio. Dwell-to-confirm and hands-on-hips-Back are
  dropped.
- **M5 — Tuning pass** — unchanged: real dancer, real play space, hand to friends.

## Not adopted from the research

- Dwell-to-confirm (rejected by the research itself and by Dance Central precedent).
- Grip / hand-state / Z-push — already ruled out on v1.
- Taking the parameter table as "validated" — see `research-thresholds.md` caveats.
