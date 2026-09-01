# Skeleton Key recogniser — staged consolidation plan

**Date:** 2026-09-01
**Inputs:** the Gemini code review (`docs/notes/code-review-report-2026-09-01.md`), verified
against the actual `gestures.cpp`; the 3 prior deep-research passes; our own live-test /
replay history (see the memory file).

## Why

The recogniser is a hand-built FSM plus **6 layered guards** and **71 config knobs**. Two
problems will not yield to more tuning (up/down oscillation, wave-back-to-park mis-firing the
opposite direction), and the guards have started to overlap and mask each other. The review
confirmed:

- **The directional reversal guard is ~dead code** — `swipeCooldownMs` (500) blocks any new
  swipe for longer than the guard's window (`revGuardMs` 450), so it only ever fires in one
  edge case. The `looksLikeReturn` confirm-delay is doing the actual return-stroke work.
- **The swipe core still triggers/segments/peak-gates on the noisy legacy EMA velocity** —
  the 1€/Savitzky-Golay stream we built to kill jitter only gates vertical, piecemeal.
- Two real bugs (`backArmed` never re-armed on grace-timeout → 0 ms Back fires; `resync`
  leaves in-flight gesture state dangling).
- The clean fix for both open problems is **positional invariants** (end-zone gates for U/D,
  midline-crossing for L/R), not more time/velocity lockouts.

**All numeric thresholds below marked "(tune)" are Gemini's engineering guesses — starting
points only. Tune against the replay corpus + one in-game pass, same caveat as every prior
research pass.**

## Ground rules (every stage)

- Working-tree only, **no commits / no push** unless Ven asks.
- Each stage must leave a **shippable build**: `build.cmd` 0/0, synth suite green, DLL sanity
  (8 Nui ordinals, base 5, machine x86, imports = KERNEL32 + USER32 + GDI32), byte-deploy to
  `E:\LegacyOfflinePC`.
- Replay corpus for regression: `tools/captures/skcap-20260901-150717.skcap`,
  `…-152922.skcap`, `…-032901.skcap`, plus `tools/captures/skcap-20260828-001358` and
  `…-230153-tightroom`.
- Stages that change felt behaviour (2, and the re-tune in 1) need a short **in-game** check
  by Ven before the guards they replace are deleted.
- No AI co-author trailers, ever.

## Recommended order: **0 → 2 → 1 → 3**, then 4 only if needed

Stage 0 is free cleanup. Stage 2 (positional invariants) is done before Stage 1 because it
directly fixes Ven's two open problems and lets us delete `vLock` + the confirm-delay; the
SG-velocity commit (Stage 1) is a bigger re-tune with lower urgency once the invariants are
catching the bad cases. Stage 1 still matters (it stops the spurious *trigger*, not just the
spurious *fire*). Gemini's own order is 1→2; either works — flag to Ven.

---

## Stage 0 — bug fixes + delete the dead reversal guard   ✅ DONE 2026-09-01 (deployed 180224 B)

**Result:** build 0/0, synth 13/13 unchanged, DLL sanity holds. Replay corpus tallies within
noise (+1 LEFT on 150717, +3 LEFT/−1 DOWN on 152922 — the reversal guard's one niche case,
no new pathology). All 4 bugs fixed, reversal guard fully removed, one duplicate config key
removed. Knobs 71 → ~66.

**Goal:** correctness, and remove one whole guard, with no intended behaviour change.

**Changes (`gestures.cpp`, `config.h`, `config.cpp`):**
1. **Bug 1.2 — `backArmed`.** In the Back grace-timeout branch (`gestures.cpp` ~L320) add
   `g.backArmed = true; g.backRepeatNext = 0;` to match the Confirm branch. Stops a 0 ms
   instant Back fire when the arm rests at `outFrac ≈ 0.18` after a previous Back.
2. **Bug 1.5 — resync.** In the `resync` branch (~L187) clear the in-flight gesture state:
   `g.swActive = g.swPending = false; g.rs = RS_IDLE; g.confirmSince = g.backSince = -1;
   g.vLock = 0;` (and `g_dbg` swipe/repeat/held fields). Prevents a stale-anchor swipe or a
   spurious repeat/Confirm on the first live frame after a >250 ms same-body tracking gap
   (`Gestures::Reset()` only runs on body-lost / tracking-ID change, not a hiccup).
3. **Bug 1.6 — `g_epoch`.** Reset it in `Gestures::Reset()` so replay `--loop` / body-switch
   log timestamps restart at 0. Trivial.
4. **Bug 1.1 — mirrored SG.** Where the horizontal SG velocity is consumed in the swipe FSM
   (`g.sgVx` at ~L458/L480/L489), use a mirror-applied value
   (`const float esgVx = m * g.sgVx;`) so it agrees in sign with `swSegDir` (which is in
   effective/mirrored coords). Vertical is already consistent. Low impact today
   (`mirror=false` default) but it's a landmine and Stage 1 leans on this signal.
5. **Delete the reversal guard.** Remove config `revGuardMs` / `revGuardPeakK` /
   `revGuardDistK`; state `swRevGuard` / `lastFireDirH` / `lastFireDirV` / `lastFireMsH` /
   `lastFireMsV`; the `&& (!g.swRevGuard || …)` clause in the fire condition; the
   `lastFireDir*/Ms*` writes in `fireSwipe()` and the RS_HOLD repeat; the `[rev]` log tag
   and `rg=` trace field. Self-contained — nothing else reads that state.

**Config:** 71 → ~66 (`-3` reversal + tidy).

**Verify:** build 0/0; synth **13/13** unchanged (if `waveback_*` shifts, investigate — it
shouldn't); replay corpus fire tallies within noise of the current deployed build; DLL
sanity; deploy.

**Risk:** low. Only real behaviour change: the "swipe opposite, immediately after releasing an
L/R hold, within 450 ms" edge case loses its raised bar — rare, and the settle gate +
confirm-delay still apply.

**Rollback:** revert the working-tree hunks.

---

## Stage 2 — positional invariants for Up/Down and Left/Right   ⏳ CODE DONE 2026-09-01 (deployed 181760 B) — DELETIONS PENDING IN-GAME CHECK

**Result on replay:** UP misfires −66 % / −80 % (150717 / 152922), LEFT ~halved, RIGHT
unchanged (real outward swipes preserved). UP↔DOWN flips <2 s ~25 → ~5; longest vertical
burst ~11 → ~5. synth 14/14 (`scroll_down_x3` → D=3 / U=0; `waveback_right_slow` no longer
mis-fires LEFT). A `swipe X rejected -- ended out of zone (ex= ey= seg0=)` diag line is logged
once per rejected stroke for tuning.

**Not yet done (needs Ven's in-game check first):** delete `vLock` + the confirm-delay. Both
are still active as a backup while the `up_end_min_y` / `left_end_max_x` / etc. numbers
(Gemini's guesses) get tuned live. Stage 1 also waits on this checkpoint so an in-game issue
isn't ambiguous between the invariants and the SG re-tune.

**Goal:** kill the two unsolved problems with end-position gates instead of time/velocity
lockouts; retire `vLock` and the confirm-delay.

**2a — Up/Down end-zone gate.** Add to the vertical fire condition (`gestures.cpp` ~L544):
- A fired **UP** requires the hand to *end* raised: `ey >= upEndMinY` (tune ≈ **+0.15**).
- A fired **DOWN** requires the segment to *start* at/above neutral and *end* low:
  `swSegStart >= downStartMaxY` (tune ≈ **−0.25**) AND `ey <= downEndMaxY` (tune ≈ **−0.40**).
- Rationale: a recovery-raise after a down-swipe starts deep-low and ends at neutral — it
  never reaches the UP end-zone, so it's rejected regardless of speed. Repeated real DOWN
  scrolling still passes (each starts near neutral, ends low).
- New config: `upEndMinY`, `downStartMaxY`, `downEndMaxY`.

**2b — Elbow-relative UP check (secondary, guarded by elbow tracked).** UP additionally
requires `dh.y >= elbow.y - upElbowMargin` (tune ≈ **0.05 torso**) — hand at/above the elbow
= a real forearm raise; hand trailing below the elbow = a recovery drag. Skip the check when
`domElbow` is not `Usable`. New config: `upElbowMargin`.

**2c — Left/Right midline-crossing gate.** Add to the horizontal fire condition:
- A fired **LEFT** (dominant right hand) requires `ex <= leftEndMaxX` (tune ≈ **−0.10**) —
  the stroke must cross the torso midline into the left half-space.
- A fired **RIGHT** requires `ex >= rightEndMinX` (tune ≈ **+0.35**) — reach outward.
- Mirror-aware (`ex` is already effective/mirrored).
- Rationale: a return-to-park from far right ends around `ex ≈ +0.2` (still your own side) →
  fails both → no fire, **zero latency, zero state**. A deliberate cross-body swipe reaches
  the zone.
- New config: `leftEndMaxX`, `rightEndMinX`.

**Then, once 2a–2c verify on replay + in-game:**
- **Delete `vLock`** — config `vLockMinMs` / `vLockSettleVel` / `vLockSettleFrames` /
  `vLockMaxMs`; state `vLock` / `vLockSettle` / `vLockFloorUntil` / `vLockUntil`; the
  per-frame lockout block (~L430) and the `vlk=` trace field. The UP end-zone gate replaces
  its job (recovery-raise ends at neutral → rejected).
- **Delete the confirm-delay** — config `swipeConfirmFrames` / `swipeReturnBand`; state
  `swPending` / `swPendFrame` / `swStartPos`; the pending-completion block (~L535), the
  `looksLikeReturn` branch, the `pnd=` trace field; simplify `fireSwipe()` call sites back to
  direct `return emit(fireSwipe())`. The midline gate replaces its job with no latency.

**Config:** ~66 → ~62 (add 6: `upEndMinY`, `downStartMaxY`, `downEndMaxY`, `upElbowMargin`,
`leftEndMaxX`, `rightEndMinX`) then −10 (`vLock*` 4, `swipeConfirm*` 2, plus `swipeVertSgConfirm`
if 2a makes it redundant, plus revisit `swipeSettleAfterMs`/`Frames`/`Frac`). Net ≈ **50**.

**Verify:**
- synth: rework `waveback_right_fast`/`_slow` to expect **no LEFT** (midline gate);
  `swipe_left`/`_right`/`_up`/`_down` still fire (make sure the synth stroke amplitudes reach
  the new end-zones — they should, they're full swipes; adjust the synth targets if not).
  Add a `scroll_down_x3` case (three down-swipes with recovery raises → exactly 3 DOWN, 0 UP).
- replay corpus: the oscillation bursts (`UP<->DOWN` flips, longest vertical burst) and the
  wave-back LEFT/UP misfires should collapse. Compare against the Stage-0 baseline numbers.
- **in-game (Ven):** does a normal LEFT/UP/RIGHT/DOWN still fire without feeling like you
  have to over-reach? Does scrolling down a list work without stray UPs? This is where the
  `*EndMinY` / `*EndMaxX` numbers get tuned.

**Risk:** medium — it's a felt behaviour change (swipes must reach into the target
half-space / zone). But it is the payoff: two clean position checks replace three guards.

**Rollback:** the invariant clauses are additive `&& (…)` terms — comment them out to revert
felt behaviour without touching the deletions yet. Do the deletions only after in-game sign-off.

---

## Stage 1 — commit the swipe pipeline to the 1€ / Savitzky-Golay stream   ✅ DONE 2026-09-01 (deployed 179712 B) — needs in-game tuning

**Done together with the vLock + confirm-delay deletions.** `ex/ey` now = 1€-filtered
position, `evx/evy` = Savitzky-Golay velocity; dominant-hand EMA retired (`g.nd` still EMA for
the Back pose). Re-tune: `swipeVelocity` 1.3→**1.0**, `swipePeakRatio` 1.4→**1.9** (a slow
arm-drop-to-rest plateaus ~1.4 torso/s on SG and was firing DOWN — the peak gate now rejects
it), `confirmStillVel` 1.2→**0.5** and `confirmRaiseFy` 0.05→**0.25** (SG "still" detection is
reliable now, so a casual raise was completing the Confirm dwell). Trace line reworked:
dropped `f1e=`/`sgv=`/`pnd=`/`vlk=`/`rg=`, added `hoe=` (hand-over-elbow). Build 0/0, synth
**14/14**, DLL sanity holds. Knobs ~66 (Stage 3 does the big prune).

**Replay deltas that need Ven's in-game read:**
- **UP↔DOWN oscillation regressed vs Stage 2** (flip 5→11/16, burst 5→8) — vLock is gone, so
  the up end-zone gate carries it alone. Still well below Stage 0 (~26/~11). Tighten
  `up_end_min_y` (0.15→0.25) or `up_elbow_margin` if it's bad in-game.
- **RIGHT dropped 62→35 on skcap-150717** — mostly *not* zone-rejects (only 9); the SG stream
  + 1.9 peak ratio filter out jitter-triggered RIGHTs. Could be correct (jitter) or
  over-rejection of real swipes. Watch for RIGHT under-firing; lower `swipe_peak_ratio` toward
  1.5 if so.
- Confirm ~stable on the two swipe-test captures; the tightroom/old-capture over-fires
  (9→0, 9→5) were fixed by the `confirmRaiseFy` bump.

**Goal:** the spurious *trigger* stops happening, not just the spurious *fire*. Retire the
legacy EMA on the dominant hand's swipe path.

**Changes (`gestures.cpp`):**
- Dominant hand **position** for swipe logic: use the 1€-filtered `g.f1eX` / `g.f1eY`
  (mirror-applied for x) in place of `g.dom.fx` / `.fy` for `ex` / `ey`, `pos`, `progress`,
  `swSegExt`, the zone checks, and the Stage-2 invariants.
- Dominant hand **velocity**: use the mirror-applied SG velocity (`esgVx = m*g.sgVx`,
  `esgVy = g.sgVy`) for the swipe trigger (~L441), segmentation `idir` (~L481), `dirVel`, and
  the ballistic peak (`swSegPeak` → track from SG). Then `swSegPeakSg` and the separate
  vertical SG-confirm become redundant — fold them out.
- Confirm stillness (`fabsf(evx/evy) < confirmStillVel`, ~L330) and raise (`ey`): same
  substitution.
- Keep `g.nd` (non-dominant, Back pose) on the EMA for now — Back's `ndSpeed` gate is coarse
  and Back isn't in the swipe pipeline. Note for a later pass.
- **Re-tune** (SG peaks run ~30–50 % below legacy, ~2-frame/~66 ms group delay):
  `swipeVelocity` 1.3 → ~**0.85** (tune); `swipePeakRatio` re-tune; `swipeSettleFrac` /
  `swipeSettleVel`-style thresholds re-scale; `revVel` (0.35·swipeVelocity) follows.
  `swipeConfirmFrames` (if it still exists) can likely shrink — SG already lags, and a
  reversal now shows cleanly.
- `swipeVxCeiling` becomes a no-op (SG has no superhuman spikes) — leave it very high or
  delete in Stage 3.

**Config:** delete `smoothFast` / `smoothSlow` (dominant-hand EMA gone; `g.nd` keeps its own
or inherits 1€), `swipeVertSgConfirm`, likely `swipeVxCeiling`. ~50 → ~**45**.

**Verify:**
- synth 13/13 with re-tuned thresholds (the swipe cases will need their expected fire frames
  re-checked; the *tallies* must stay identical).
- replay corpus: the headline metric is **horizontal jitter misfires** — count spurious
  LEFT/RIGHT in the quiet stretches; should drop toward zero. Also confirm real swipes still
  fire and latency is acceptable (SG adds ~66 ms; total emit latency should stay < ~200 ms).
- in-game (Ven): responsiveness check — does it still feel prompt?

**Risk:** medium — it's a re-tune, and every swipe threshold shifts at once. Mitigation:
optional `swipe_signal = sg | legacy` config switch kept for one iteration so we can A/B and
roll back live; remove it once SG is trusted. (Costs ~a dozen lines of branching — Ven's
call whether it's worth it vs. just relying on the working-tree revert.)

**Rollback:** revert the substitution hunks; the 1€/SG code stays (it's still the parallel
stream + Stage-2 position source).

---

## Stage 3 — config prune + documentation

**Goal:** get the knob count honest and the docs current.

- Sweep `config.h` for anything now dead or dominated: the settle gate
  (`swipeSettleAfterMs` / `swipeSettleFrames` / `swipeSettleFrac`) is a candidate — after
  Stages 1–2 a return / perpendicular stroke is rejected by the midline/zone gate or the
  axis-ratio, so the 1200 ms post-swipe settle window may no longer earn its place. **Verify
  by replay before removing.**
- `swipeVxCeiling` delete (Stage 1 made it inert).
- Target **~22–25** documented knobs, grouped: signal (1€/SG), swipe trigger, segmentation,
  U/D zones, L/R zones, Confirm, Back, output keys, dev.
- Rewrite `docs/build-plan.html` — it has described a superseded "v0.7 positional model" for
  many sessions. New pass covers the actual recogniser: 1€→SG signal, segmented swipe,
  positional invariants, Confirm/Back dwell, gameplay mute, the harness.
- Refresh `dist/kinectnav.example.ini` to the surviving keys with real defaults.

**Risk:** low (deletions of verified-dead knobs + docs).

---

## Stage 4 — energy-bounded segmenter (OPTIONAL, only if whack-a-mole recurs)

The current model splits an already-triggered swipe at velocity reversals. Gemini's and
Pass 3's target is a **continuous energy-bounded segmenter**: open a stroke when
`|v_sg| > openVel` (~1.2 tune), track trajectory + peak + start, close it at the next
`|v_sg| < closeVel` (~0.4 tune) zero-crossing, then a **geometric classifier**
(`atan2(Δy,Δx)` direction + net travel + the Stage-2 positional invariants) emits the action.
Confirm/Back stay as the separate dwell-pose engine.

- **`$P+` / point-cloud template matching is explicitly NOT recommended** — for 4 cardinal
  strokes a direction is an `atan2`, and Confirm/Back are static holds a point-cloud matcher
  can't represent. Both Gemini and our own earlier read agree.
- Do this only if, after Stages 0–3, the segment model still needs per-case patching. If
  Stages 2 + 1 hold, the current model + invariants may simply be sufficient.

---

## Knob trajectory

| after stage | approx knob count | guards remaining |
|---|---|---|
| now | 71 | 6 (cooldown, settle, L/R hold, reversal, vLock, confirm-delay) |
| 0 | ~66 | 5 (reversal deleted) |
| 2 | ~50 | 3 (cooldown, settle, L/R hold) + 2 positional invariants |
| 1 | ~45 | same, on a clean signal |
| 3 | ~22–25 | settle gate re-evaluated; docs current |
