# Code review + engineering-opinion request — Skeleton Key gesture recogniser

**For:** Gemini (a second AI reviewer). This is a **code review and architecture opinion**,
not a literature survey. Two prior deep-research passes already covered the domain and the
"ideal" architecture (summarised below) — do **not** just repeat "use a $P+ template
matcher". Look at the actual code that will be pasted after this brief and give a candid
engineering assessment.

---

## 1. What the code is

**Skeleton Key** — a drop-in `Kinect10.dll` proxy that adds hands-free menu navigation to a
dance video game (menus only, never gameplay). It taps the game's skeleton stream, runs a
gesture recogniser, and synthesises arrow / Enter / Esc keystrokes.

The file under review is **`gestures.cpp`** (~590 lines) — `Gestures::Update(nav, frameMs)`
is called once per skeleton frame (~30 Hz) on a single thread and returns a `GestureAction`
(None / Left / Right / Up / Down / Confirm / Back). Supporting files also pasted:
`config.h` (the `Config` struct — currently **71 tunable constants**, all overridable from an
INI), `filter.h` (1€ filter + Savitzky-Golay), and `recognizer.cpp` (frame pump + navigator
selection + `GestureAction` → keystroke).

**Constraints that rule out a lot of "standard" answers:**
- Microsoft **Kinect v1**, 20-joint skeleton, **~30 Hz**, skeleton joints only (no image
  processing on the table).
- **Hands are usually "inferred", not "tracked"** — ±0.1–0.3 m jitter, frame-to-frame
  velocity spikes to ~9 m/s (~20+ torso/s) on a single bad frame.
- **Frontal plane only** — depth (toward/away) is unusable on distal joints. No "push".
- Rock-solid: shoulder-centre, spine-base, head. Moderate: elbows. Worst: hands/wrists.
- Everything normalised by **torso length** (`|shoulderCentre − spineBase|`, ~0.41 m);
  distances below are "torso units", speeds "torso/s".
- **Real deployment ~2 m from the sensor, cluttered room** — worst case for a v1 skeleton.
- **One developer. No training-data pipeline. Every threshold must stay human-readable and
  INI-tunable** (tuning is done by hand against recorded `.skcap` sessions replayed through
  the same recogniser).
- Latency budget for discrete menu commands ~150–250 ms.
- Product decision already made by the owner: **navigation stays discrete swipes — no
  hold-to-repeat for Up/Down** (Left/Right does have a hold-to-repeat). Don't recommend
  adding vertical hold-to-repeat; assume it's off the table.

## 2. The current gesture model (what the code implements)

### Signal conditioning
- Per hand: a **dual-rate exponential smoother** (fast α≈0.5 for velocity, slow α≈0.15 for
  held-pose), velocity by central difference on the fast track off frame timestamps.
- **Added recently, running in PARALLEL, not yet consumed by the recogniser's decisions
  except where noted:** a **1€ filter** on the raw dominant-hand x/y and a **5-point
  Savitzky-Golay** first-derivative off that (`filter.h`). Logged next to the legacy
  velocity. Replay analysis showed the 1€→SG stream removes essentially all the >16 torso/s
  jitter spikes and roughly halves p99 velocity, at the cost of ~1 frame lag and ~30–50%
  peak attenuation on real strokes. A few recogniser gates now *also* consult the SG stream
  (`swipeVertSgConfirm`, the reversal-guard "decisive" test) but the core swipe trigger
  still runs on the legacy velocity.

### Left / Right / Up / Down — "segment-based swipe"
A swipe becomes active when one axis's (legacy) velocity crosses `swipeVelocity` (1.3) and
dominates the other axis by `swipeAxisRatio` (1.4); vertical additionally needs the hand in
front (`|ex| < swipeVertXBand` 0.55) and inside a y-band (`swipeYMin`/`swipeYMax`).

Once active the motion is **split into segments at every velocity reversal** (crossing
±`0.35·swipeVelocity` opposite the current segment). Only the **current** segment is judged,
and its travel is measured **from the point the hand last turned around**. Idea: a small
"countersteer" flick before a real stroke is just segment 1 (short in the eventual
direction), and the real stroke is segment 2.

A segment fires when: `≥ swipeMinFrames` (2) frames, `progress ≥ swipeDistance` (0.28) in
the segment direction, still moving that way, **legacy** peak speed this segment `≥
swipeVelocity·swipePeakRatio` (1.82), and (vertical only) the **SG** peak this segment `≥
swipeVertSgConfirm` (0.9) in the fired direction. Aborts on `swipeMaxActiveFrames` (24),
`swipeMaxSegments` (4), leaving the vertical zone, or a first-segment stall.

### Guards layered on top of the swipe trigger (each added to fix a specific observed failure)
1. **Cooldown** (`swipeCooldownMs` 500) — no new swipe right after one fires.
2. **Settle gate** (`swipeSettleAfterMs` 1200 window; leaky counter `swipeSettleFrames` 3 at
   `swipeSettleFrac·swipeVelocity`) — enforced *only* in the post-swipe window: the hand
   must have been slow for a few frames before the next swipe, to stop a return/recovery
   stroke firing a second swipe. (An always-on version was tried and starved on inferred-hand
   velocity noise.)
3. **L/R hold-to-repeat** state machine (`RS_IDLE`/`RS_POST`/`RS_HOLD`): after an L/R swipe,
   a 400 ms window to catch "arm extended straight out + roughly horizontal"
   (`armExtendFrac` 0.90, `armLevelTanMax` 1.0) → accelerating auto-repeat of that arrow key
   until the arm relaxes or swings off horizontal. Swipes are fully blocked while
   `rs != RS_IDLE`.
4. **Directional reversal guard** (`revGuardMs` 450, `revGuardPeakK` 1.7, `revGuardDistK`
   1.6): after a swipe (or L/R repeat) fires in direction D on an axis, an *opposite*-
   direction swipe on that axis within the window must be **decisive** — SG segment peak ≥
   `swipeVelocity·revGuardPeakK` **and** travel ≥ `swipeDistance·revGuardDistK` — or it's
   suppressed. Intent: the lazy "hand drifts back to parking after a right-swipe, looks like
   left" stroke fails it; a deliberate snap-back passes. L/R repeats keep the guard armed
   through the hold.
5. **Vertical post-swipe lockout** (`vLockMinMs` 500 / `vLockSettleVel` 0.55 /
   `vLockSettleFrames` 4 / `vLockMaxMs` 1400): after a U/D fires, **all** vertical swipes are
   blocked until the SG vertical speed stays below `vLockSettleVel` for `vLockSettleFrames`
   frames past a hard `vLockMinMs` floor, or the `vLockMaxMs` failsafe. Intent: kill the
   up/down "limit cycle" where the arm falling back after a down-swipe fires UP and the
   rebound fires DOWN. This is a blunt time/stillness block — it can eat a genuinely fast
   deliberate reverse. It's exposed as a 3-knob dial (tight ↔ loose) because there is no
   clean separator (see §3).
6. **Swipe-confirm delay** (`swipeConfirmFrames` 8, `swipeReturnBand` 0.35): a swipe that
   **opens with the hand already out past `swipeReturnBand` and moving back toward centre**
   ("looks like a park return") is held pending for `swipeConfirmFrames` frames; a reversal
   in that window (segmentation starts a new segment → clears `swPending`) cancels the emit.
   A normal outward swipe from near centre is never delayed.

### Confirm (Enter)
Dominant hand raised (`ey > confirmRaiseFy` 0.05 — very low) + roughly still
(`|v| < confirmStillVel` 1.2), not mid-swipe, `rs == IDLE`, and **no Back pose active**.
First fire after `confirmDwellMs` (1800) with `dwellGraceFrames` (20) tolerance, then
auto-repeats every `confirmRepeatMs` (550) after a `confirmRepeatFirstMs` (900) grace.

### Back (Esc)
Non-dominant arm down-and-out to the side ~45° (`backOutMin` 0.30, `backDownMin` 0.22 <
downFrac < `backDownMax` 0.88, still). `backDwellMs` 1800 then auto-repeats. **Evaluated
before Confirm** and hard-disarms Confirm while the Back pose is held (the dominant hand
drifts up when you raise the other arm, which was firing Confirm instead of Back).

### Gameplay mute
Detected from the game's own file I/O (song-bundle load burst then file-quiet). Currently
**off by default** — arrows/Enter do nothing inside a routine anyway, and Back is off by
default too.

## 3. The open problems we cannot cleanly solve

1. **Up/Down oscillation.** When the user does repeated discrete down-swipes to scroll a
   list, the arm-raise *between* strokes (recovery) is a full-amplitude, fast motion — it is
   kinematically **identical** to a deliberate UP. We tried: a settle-time lockout, a
   "decisive reverse" raised bar, and a "must return past the stroke's origin" positional
   gate. The oscillation defeats all of them because the recovery swings really are
   full-size strokes; the *only* thing that reliably separates them is "did the hand come to
   a stop", which forces a blunt time block that eats a fast deliberate reverse. Current
   state = the `vLock` dial, middle setting, ~50% reduction.
2. **Wave-back / return-to-park mis-firing the opposite direction.** Hand parked out right →
   waved back toward the torso → fires LEFT before the hand returns. The swipe-confirm delay
   (guard 6) catches brisk wave-backs but a slow, full-amplitude wave to the torso still
   reads as a real LEFT — arguably because it *is* a ~1-torso leftward motion. A "home
   anchor" model (a leftward move only counts as LEFT if it ends left of where the hand
   normally rests) has been floated but not built.
3. **Whack-a-mole.** Every guard above was added in response to one specific live-test
   failure, and several interact (settle gate vs reversal guard vs confirm delay vs vLock
   all police overlapping "return stroke" situations). 71 config constants. Some earlier
   knobs are suspected dead or masked. The owner is asking whether this is still a sound
   design that needs consolidation, or whether it has accumulated past the point where it
   should be rebuilt.

## 4. Prior research (don't just repeat these)

- Pass 1: biomechanics of mid-air interaction (Consumed Endurance / gorilla arm, swipe-not-
  dwell, body-anchored positional model). Numbers were uncited engineering defaults.
- Pass 2: a ranked Back-gesture shortlist (winner: contralateral downward slash) and a
  "quiescence precondition gate" for engaging mid-dance.
- Pass 3 (architecture): recommended scrapping the FSM for a **1€ filter → Savitzky-Golay
  velocity → energy-bounded segmenter → `$P+` point-cloud template matcher**, with an
  optional NDH-on-hip clutch. Only **Phase 1 (1€ + SG, parallel)** has been implemented; the
  segmenter + `$P+` (Phases 2–3) have not. `$P+` for a 4-direction + 1-diagonal vocabulary
  is arguably overkill (a segmented stroke is basically a short line; direction is an
  `atan2`), so the value there is really the **segmentation**, not the template matcher.

## 5. What we want from your review

Read `gestures.cpp` / `config.h` / `filter.h` / `recognizer.cpp` and give a candid
engineering opinion:

1. **Correctness / bug hunt.** Concrete defects, off-by-ones, unreachable branches, state
   that isn't reset on `Reset()` / navigator change, ordering hazards between the stacked
   guards, cases where two guards fight and produce a surprising result. Anything that would
   misfire or fail to fire that we haven't already listed in §3.
2. **The guard stack.** Are guards 1–6 a coherent set or are they redundant/conflicting? Is
   there a smaller number of mechanisms that would cover the same failure modes? Which knobs
   are likely dead or dominated by another guard? Is the parallel legacy-EMA + 1€/SG
   arrangement sound, or should the recogniser commit to one velocity source?
3. **The two unsolved problems (§3.1, §3.2).** Given the constraints (discrete vertical
   swipes only, no training data, INI-tunable, v1 noise), is there a mechanism we're
   missing? Assess the "home anchor" idea. Is "a big committed motion toward the torso is
   indistinguishable from a swipe" actually fundamental, or is there a feature (elbow angle,
   the SG vs legacy disagreement, trajectory curvature, dwell-before) that separates intent
   from a return?
4. **Refactor vs rebuild.** Is this FSM-plus-guards salvageable with consolidation, or has it
   hit the wall Pass 3 predicted? If consolidation: what specifically would you merge or
   delete? If rebuild: what is the **minimum** viable version of the segmenter approach for
   *this* vocabulary (note `$P+` may be overkill), and a concrete migration order that keeps
   a working build at every step.
5. **The single highest-leverage change** you'd make right now.

Be specific and code-level. Where you assert a number or a technique, say whether it's a
measured result or an engineering convention. Call out where a constraint in §1 makes a
suggestion impractical rather than recommending it anyway. Prefer deleting code to adding it.
