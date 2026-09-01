# Recognition-architecture research — digest (Deep Research pass 3)

Source: `docs/Kinect Gesture Recognition Architecture.md` (Gemini Deep Research, 2026-09-01).
Brief that produced it: `docs/notes/research-brief-2026-09-01-movement-modeling.md`.
Raw export (HTML + formula PNGs): `docs/Kinect Gesture Recognition Architecture/`.

## Verdict / trust level

Same shape as passes 1 and 2: **the concepts and named algorithms are real and well-chosen;
the bibliography is padded with junk** (a guitar-pedal saturator blog, a seismology paper, a
"Topics by Science.gov" scrape, an arXiv id dated 2607). The load-bearing citations —
1€ filter (Casiez et al. CHI 2012), `$P` / `$P+` (Vatavu & Anthony & Wobbrock), Machete
(Taranta et al. TOCHI '21), Consumed Endurance (Hincapié-Ramos et al. CHI 2014),
minimum-jerk / two-thirds power law (Flash & Hogan; Lacquaniti, Terzuolo, Viviani), Kendon /
McNeill gesture phases, anticipatory postural adjustments (Bouisset & Zattara) — are all
genuine and correctly described. Numbers below are again mostly the report's own engineering
conventions, not measured values — treat as starting points, tune against captures.

## Decoded formula images (they're greppable now, inline in the .md)

| Was | Value | Meaning |
|---|---|---|
| jitter floor | **±0.1 – 0.3 m** | inferred-hand high-frequency noise |
| impossible-velocity artifact | **9.0 m/s** | one 0.3 m inferred snap ÷ 33 ms frame |
| frame time | **33 ms** | 30 Hz |
| 1€ filter `f_cmin` | **1.0 Hz** | min cutoff — kills resting jitter |
| 1€ filter `β` | **0.05** | speed coefficient — opens cutoff during a stroke so peaks pass lag-free |
| `$P+` accuracy | **>95%** | with 3–5 templates per command |
| S-G window | **5 frames ≈ 166 ms** | polynomial-fit low-pass differentiator for velocity |
| perceptual latency budget | **150–250 ms** | tolerated for discrete mid-air commands |
| two-thirds power law | `v(t) = c·κ(t)^(−1/3)` | tangential velocity vs path curvature |

## The core recommendation

**Replace the FSM + ~40 thresholds with a Hybrid Segmenter → Template-Matcher pipeline:**

1. **Signal conditioning.** Drop the dual-rate EMA. Run a **1€ filter** on raw wrist X/Y
   (`f_cmin ≈ 1.0 Hz`, `β ≈ 0.05`). Derive velocity with a **5-point Savitzky-Golay**
   filter, not central difference — the S-G window bridges 1–2 frame inferred spikes so the
   9 m/s artifacts never enter the velocity stream (this is the direct fix for our
   "spurious segment splits / direction misfires").
2. **Energy-bounded segmenter.** Watch S-G velocity magnitude of the active wrist. Open a
   segment when it breaks a (dynamic) noise-floor threshold; close it at the next velocity
   zero-crossing (the post-stroke hold). That closed segment is the **stroke nucleus** —
   preparation and retraction are structurally outside it, so no cooldown hacks needed.
   Retraction rejection falls out for free: it's a *later* segment, and it scores badly
   against templates anyway.
   - Heavier option: **Machete** (Taranta et al.) does segmentation + recognition jointly
     from a single template, bounded memory. Report says only reach for it if the simple
     energy-bounded segmenter proves inadequate.
3. **`$P+` point-cloud recognizer** (Vatavu) classifies the segment. Gesture = a
   position/scale/rotation-normalized 2D **point cloud**, matched by nearest-point distance,
   *not* a time sequence. Consequences that matter for us:
   - a 0.2 m tired wave and a 0.6 m big swipe normalize to the same cloud → **amplitude
     drift and wave-vs-swipe stop mattering**;
   - matches on spatial proximity not temporal order → **APA counter-motions and speed
     variation don't break it**;
   - ~100 LOC, needs **3–5 recorded exemplars per command**, tuning = re-record a gesture
     into the config, no metric-threshold guessing.
4. **Clutch** to beat the Midas touch while dancing: **quiescence precondition + anatomical
   anchor** — non-dominant wrist held on the hip (spine-base joint) AND dominant hand
   kinetic energy below threshold for 300–500 ms. Release the anchor = disengage instantly.

## Motor-control points worth keeping

- **Gesture phrase = preparation → pre-stroke hold → stroke → post-stroke hold →
  retraction** (Kendon/McNeill). Only the **stroke** carries the command; it's the part with
  high kinetic energy and a stereotyped bell-shaped (often asymmetric — sharp accel, long
  decel) velocity profile. Design the recognizer to isolate the stroke, not to gate on
  positions.
- **The countersteer is an Anticipatory Postural Adjustment** — a real, named phenomenon
  (Bouisset). Its *direction can invert* with the user's lean, so it is **useless as a
  predictive cue** and fragile to segment on. The report's answer: don't special-case it at
  all — a point-cloud matcher absorbs the "hook", and an energy segmenter treats it as
  pre-stroke buildup and starts the real stroke at the zero-crossing after it. This
  retroactively explains why every windup-window hack we tried (rounds 3a–3e) failed.
- **Minimum-jerk / two-thirds power law as an intent prior.** Real strokes are smooth and
  obey `v ∝ κ^(−1/3)`; Gaussian sensor noise and incidental bumps don't. Optional extra
  filter: score a segment's smoothness, reject if it grossly violates minimum-jerk.
- **Wave vs swipe → neither.** Pure wrist-wave is below the v1 noise floor at 2 m; whole-arm
  swipe is gorilla-arm. Target the middle: **elbow loosely anchored at the side, forearm
  pivots**, ~0.2–0.3 torso of hand travel. (This is exactly the elbow-relative direction
  already half-built in `gestures.cpp` — `navFromElbow`.)
- **Fitts / list nav:** absolute hand-position → list-index mapping is out (overshoot, no
  proprioceptive precision). Discrete strokes for short lists, rate-controlled hold-to-scroll
  for long — which is what we already do.
- **Latency:** 150–250 ms is fine for discrete commands, so the 166 ms S-G window + template
  match fits the budget. Audio "tick" on recognition is processed ~30 ms faster than visual
  and stops users over-extending out of doubt — worth adding to the overlay/output path.

## The proposed 6-command vocabulary — and where it fights decisions already made

| Command | Report's definition | Conflict with current build |
|---|---|---|
| **Engage (clutch)** | NDH wrist held on hip 300–500 ms + DH quiescent | **We deleted engage state in v0.8 and gave up the gameplay mute.** Report makes the clutch central to beating dance false-fires. Re-introducing it is a real reversal — worth deciding deliberately. |
| **Nav U/D** | DH forearm pivots vertically, elbow anchored | matches elbow-relative nav intent |
| **Nav L/R** | DH forearm pivots horizontally (in across body / out away) | matches; note "Left = inward, Right = outward" is a cleaner mapping than mirror-x |
| **Confirm** | NDH (while clutched on hip) flicks up/forward, returns to hip | **Current Confirm = DH raised overhead + 1.8 s dwell.** Report's version can't collide with nav (different hand) and has no Midas-touch dwell. |
| **Back** | DH contralateral slash — diagonally down-and-across to opposite hip | matches pass-2 winner and Ven's "instinct is below"; diagonal (uses X and Y) can't be confused with cardinal nav by `$P+` |

Vocabulary design rules the report gives (useful as a checklist): **spatial orthogonality**
(nav paths geometrically distinct), **anatomical independence** (Confirm/Back not on the nav
hand), **stroke primacy** (no retraction retraces a valid command path).

## Suggested migration path (report's 3 phases, mapped to our tree)

**Phase 1 — signal conditioning, run in parallel, no behaviour change.**
- New `filter.{h,cpp}`: 1€ filter + 5-point S-G. Feed raw wrist X/Y from `gestures.cpp`.
- Log old central-difference velocity next to S-G velocity in the trace line. Replay the
  existing captures (`tools/captures/`, tonight's `skcap-20260901-*`) and confirm the 9 m/s
  spikes vanish from the S-G stream. Pure instrumentation — safe to land first.

**Phase 2 — `$P+` offline.**
- `pointcloud.{h,cpp}`: `$P+` (~100 LOC). Template store = a text file of point lists
  (fits the "tune via config" constraint).
- Record 5×6 exemplars via `SkeletonKeyLab --record` (this is the whole "training set" — not
  a pipeline). Hand-clip strokes from replays, feed to `$P+`, verify >95% vs templates.
- Extend `tools/synth/synth_skcap.py` with point-cloud cases.

**Phase 3 — continuous segmentation + live.**
- `segmenter.{h,cpp}`: energy-bounded (S-G velocity breaks noise floor → open; zero-crossing
  → close) feeding `$P+` → `GestureAction` → existing `Output::TapKey`.
- NDH hip-anchor clutch with 300 ms 1€-stabilised quiescence (revives/repurposes the
  quiescence-gate code from the earlier positional design).
- One config knob: the velocity-bounding threshold for the lowest-energy acceptable wave.
- Keep the FSM path behind a config flag until the pipeline is proven in-game.

## Open questions for Ven before starting

1. **Clutch (engage) is back on the table.** The report's whole anti-Midas story depends on
   the NDH-on-hip clutch. You'd removed engage state and the gameplay mute deliberately.
   Adopt the clutch, or keep clutch-free and lean on `$P+` + energy-gating alone to reject
   dance motion?
2. **Confirm on the non-dominant hand** (flick from the hip clutch) vs. keep the current
   overhead-raise Confirm?
3. Is a from-scratch pipeline rebuild the scope you want now, or a **partial adoption** —
   e.g. just Phase 1 (1€ + S-G) dropped under the existing FSM to kill the jitter misfires,
   and hold `$P+` for later?
