# Back gesture + dance-segmentation — research digest

Source: `docs/research/Kinect Gesture Design and Segmentation.md` (Gemini Deep Research, 2026-09-01).
Numbers are again mostly locked in rendered-formula images; the legible/prose ones are below.
Treat every number as provisional (same caveats as the first research doc).

## Back / Cancel gesture — ranked

| # | Gesture | Motion | Why it doesn't collide | Fatigue | FP risk | Conf. |
|---|---|---|---|---|---|---|
| **1** | **Contralateral downward slash** | Right wrist diagonally down-and-**across the body midline** toward the LEFT hip, then hold. | Crosses the midline (disengage is straight-down / ipsilateral); enters the negative-X half-plane (not Nav Down); ends below the lower nav boundary (not Nav Left). | Very low — gravity + shoulder adduction | Very low — you don't cross your own midline on a casual arm-drop | 94% |
| 2 | Chop-and-bounce | Fast down, then reverse **up**, arresting *above* hip level. | Dynamic rebound above the pelvic plane vs. a static low hold = disengage; velocity signature (down then up) vs. sustained displacement = Nav Down. | Low | Low-Moderate — needs tuned velocity/reversal thresholds so a sloppy arm-drop isn't parsed | 86% |
| 3 | Hand to own shoulder/clavicle | Right hand retracts to the right shoulder joint, hold. | Below head (not Confirm); collapses forearm↔upper-arm distance, very close to the shoulder joint (not Nav Up/Right). | Very low — arm tucked to ribs | Very low | 82% |
| 4 | Arm straight out to the side, held | Full lateral horizontal extension at shoulder height. | Past max nav offset + elbow-angle; stays at shoulder height (not disengage). | **Moderate** — sustained extended arm | Low | 76% |

**Recommendation: Rank 1.** Honours Ven's "downward" instinct (it *is* down), just adds
"across the body" to dodge the disengage idiom. Legible detail from the doc:
- fire when `wrist.x ≤ spine.x − 0.20·torso` (0.20 torso left of midline) **and**
  `wrist.y > hip.y + margin` (stays above the hip — not a disengage)
- during the move: negative X **and** negative Y velocity (diagonal down-across)
- dwell in the target box ~200 ms (6 frames @ 30 Hz) → emit Esc, reset the origin
- Dance Central precedent: cancel was "opposite hand sweep across the lower-left corner" —
  same lower-contralateral region, we just do it with the dominant hand.

## Segmentation backstop (complements the song-render mute already shipped)

The render-call mute covers active gameplay; it can miss song-end / results / loading /
pause. Add an **intrinsic skeleton gate** so dance motion can't latch an engage.

**Quiescence Precondition Gate** — before the engage pose is even evaluated:
- weighted whole-body kinetic energy (trunk joints weighted heaviest) below a floor, AND
- SpineBase (hip-centre) translational velocity below a bound,
- both sustained ~300 ms (9 frames).
A dancer's steps / weight-shifts / bobbing keep trunk velocity above the bound → cannot
engage mid-routine even if the hands momentarily still. Violating the gate resets the
engage timer.

**Non-dominant hand:** don't require it *stationary* (fails when the user rests or is
winded) — just bound it: below the chest line + a relaxed velocity ceiling.

**Prior art:** Vogel-Balakrishnan 3-state clutch (Ambient → Pointing → Selecting) is the
model. Dance Central required arm-in-anchor for >1 s with a head-to-toe "helper frame"
(stationary check). Xbox 360 Guide gesture = left arm at a deliberately arbitrary angle,
down-and-away, held 1.5 s — arbitrary + long dwell chosen precisely to survive sports/dance.
MS Kinect HIG: dwell engagement needs progressive visual feedback (radial fill) so users can
pull away before it latches — our overlay's `ENGAGING %%` already does this.

## Proposed build order
1. **Quiescence gate** — new pre-engage stability check in `gestures.cpp` (weighted joint
   energy + hip-centre velocity, 9-frame window). Needs per-joint velocity, which means
   tracking prev positions for more than the two hands — small `HandSmooth`-style additions
   or a ring of recent frames.
2. **Back = contralateral slash** — new detector, `GestureAction::Back` already in the enum
   and `keyBack` already mapped; just needs the motion logic + `enableBack` back on.
3. Synthetic cases: `dance_no_engage` (jittery whole-body motion → never engages),
   `back_slash` (engage → down-across-left → Esc), and a `back_not_disengage` guard.
