# Code Review & Engineering Assessment — Skeleton Key Gesture Recogniser

**Date:** 2026-09-01  
**Reviewer:** Gemini (Second AI Reviewer)  
**Target Files:** `src/SkeletonKey/gestures.cpp`, `config.h`, `filter.h`, `recognizer.cpp`  
**Harness / Replay:** `tools/synth/synth_skcap.py`, `tools/README.md`

---

## Executive Summary

Skeleton Key's current recogniser has accumulated **71 configuration knobs** and **6 layered guard mechanisms** that patch overlapping failure symptoms. While the individual intent behind each guard is clear from session notes, the system currently suffers from:
1. **Critical Logic & State Bugs:** A coordinate mirroring bug on Savitzky-Golay velocity, a state lockout in `Back` dwell handling, stale state across segmentation reversals in `swRevGuard` and `looksLikeReturn`, and an unreset static `g_epoch`.
2. **Dead & Redundant Guards:** The 450 ms reversal guard is mathematically dead code masked by the 500 ms swipe cooldown; `vLock` strictly dominates the settle gate for vertical gestures.
3. **Dual-Velocity Stream Conflict:** The core recogniser triggers on a noisy backward-difference velocity that amplifies 1-frame inferred joint jitter to 11+ torso/s, while SG velocity is only checked piecemeal.
4. **Clean Solutions for Open Problems:**
   - **Up/Down Oscillation:** Discriminated by vertical bounds ($y_{\text{end}} \ge +0.15$ for UP; $y_{\text{start}} \ge -0.25, y_{\text{end}} \le -0.40$ for DOWN) and elbow-relative orientation ($y_{\text{hand}} \ge y_{\text{elbow}}$).
   - **Return-to-Park / Wave-Back:** Discriminated by the Midline Crossing invariant ($ex_{\text{end}} < -0.10$ for cross-body LEFT on right hand).
5. **Architectural Recommendation:** Do **not** implement a full `$P+$` template matcher for 4 cardinal directions and 2 static holds. Commit fully to the 1€ + Savitzky-Golay pipeline, implement energy-bounded segmentation with spatial invariants, and prune $>30$ dead config knobs.

---

## 1. Concrete Defects & Bug Hunt

### 1.1 `g.sgVx` Mirror Sign Mismatch (Breaks Horizontal SG Checks when `mirror = true`)
* **Location:** [`gestures.cpp:458`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L458), [`gestures.cpp:480`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L480), [`gestures.cpp:497`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L497)
* **Defect:** `g.sgVx` is computed in raw (unmirrored) camera coordinates (`drx/dt`). In [`gestures.cpp:208-212`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L208-L212), `ex` and `evx` are flipped by `m` (`c.mirror ? -1.0f : 1.0f`), and [`gestures.cpp:235`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L235) correctly mirrors it for debug display (`g_dbg.sgVx = m * g.sgVx`). However, inside the swipe FSM:
  ```cpp
  g.swSegPeakSg = (g.swAxis == 1) ? g.sgVx : g.sgVy; // Line 458: NO mirror 'm'!
  const float sgAxisVel = (g.swAxis == 1) ? g.sgVx : g.sgVy; // Line 480: NO mirror 'm'!
  ```
* **Impact:** `g.swSegDir` is in mirrored/effective coordinates, while `sgAxisVel` is unmirrored. At line 497, when `swSegDir > 0` (user swiping right), `sgAxisVel` is negative. `g.swSegPeakSg` tracks the maximum of negative numbers instead of the signed peak. If `swRevGuard` is tested on a mirrored horizontal swipe ([`gestures.cpp:556`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L556)), `fabsf(g.swSegPeakSg)` stays near `0.0`, and valid mirrored horizontal snap-backs are unconditionally rejected.

### 1.2 `backArmed` Permanent Lockout & 0-ms Instant Fire
* **Location:** [`gestures.cpp:304-323`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L304-L323)
* **Defect:** In `Confirm` ([`gestures.cpp:355-356`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L355-L356)), when the pose breaks and grace elapses, `confirmArmed` is reset to `true`:
  ```cpp
  if (++g.confirmGrace > c.dwellGraceFrames)
  { g.confirmSince = -1; g_dbg.confirmHeldMs = 0; g.confirmArmed = true; g.confirmRepeatNext = 0; }
  ```
  In `Back` ([`gestures.cpp:320`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L320)), when grace elapses:
  ```cpp
  if (++g.backGrace > c.dwellGraceFrames) { g.backSince = -1; g_dbg.backHeldMs = 0; } // Missing backArmed = true!
  ```
  `backArmed` is *only* reset if `outFrac < 0.15f` ([`gestures.cpp:322`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L322)).
* **Impact:** If the user fires Back and lowers their arm straight down to rest, `downFrac` exceeds `backDownMax` (0.88), breaking the pose. But if their hand rests at `outFrac = 0.18` (common when the arm hangs naturally against the hip), `outFrac < 0.15` is never satisfied. `g.backArmed` remains `false` forever. When the user later enters the Back pose again:
  - `first` is `false` (`!g.backArmed`).
  - `repeat` evaluates to `true` on **frame 1** because `frameMs >= g.backRepeatNext` (timestamp from earlier is `<= now`).
  - **Back fires instantly on frame 1 with 0 ms dwell.**

### 1.3 `swRevGuard` Stale Evaluation Across Segment Turnarounds
* **Location:** [`gestures.cpp:464-466`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L464-L466), [`gestures.cpp:481-492`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L481-L492)
* **Defect:** `g.swRevGuard` is evaluated once when `swActive` opens on Segment 1 based on Segment 1's direction:
  ```cpp
  g.swRevGuard = (axLastDir != 0 && g.swSegDir == -axLastDir && frameMs - axLastMs < c.revGuardMs);
  ```
  When the hand turns around into Segment 2 (the actual stroke), `g.swSegDir` flips (`idir`), but `g.swRevGuard` is **not updated**.
* **Impact:**
  - **False Penalty on Continues:** If a user performs a second right-swipe with a small leftward counter-flick, Segment 1 is Left (`swRevGuard = true`). When Segment 2 flips Right (same direction as last fire), `swRevGuard` remains `true`. The second right swipe is hit with the +60% distance / +70% peak velocity penalty and often fails to fire.
  - **Reversal Guard Evasion:** If the user winds up in the same direction as the last fire before snapping opposite, Segment 1 sets `swRevGuard = false`. Segment 2 flips opposite, but `swRevGuard` stays `false`, allowing the return stroke to bypass the guard entirely.

### 1.4 `looksLikeReturn` Stale Anchor (`swStartPos`) on Segment Turnarounds
* **Location:** [`gestures.cpp:454`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L454), [`gestures.cpp:563-565`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L563-L565)
* **Defect:** `g.swStartPos` is recorded at the moment `swActive` triggers (Segment 1).
  ```cpp
  const bool looksLikeReturn = c.swipeConfirmFrames > 0
      && fabsf(g.swStartPos) > c.swipeReturnBand
      && g.swSegDir * g.swStartPos < 0.f;
  ```
* **Impact:** If a motion starts near center (`ex = 0.1`), swings outward to `ex = 0.5` without firing (Segment 1), and then returns inward (Segment 2), Segment 2 is an inward return stroke from `0.5`. However, `g.swStartPos` is still `0.1` (`< swipeReturnBand`). `looksLikeReturn` evaluates to `false`. The return stroke fires immediately with 0 confirm delay.

### 1.5 Resync Leaves In-Flight State Dangling
* **Location:** [`gestures.cpp:184-195`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L184-L195), [`gestures.cpp:248`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L248)
* **Defect:** If tracking drops for $> 250\text{ ms}$, `resync` triggers and resets `oeX, oeY, sgX, sgY` and sets `g.frames = 1`. However, `g.swActive`, `g.vLock`, `g.rs`, `g.confirmSince`, and `g.backSince` are **not cleared**.
* **Impact:** For the next 11 frames (`g.frames < c.armAfterFrames`), `Update` returns early. On frame 12, `g.swActive` is still `true`, but `g.swSegStart` and `g.swFrames` are from seconds ago across a positional discontinuity.

### 1.6 Static Global `g_epoch` Persists Across `Reset()`
* **Location:** [`gestures.cpp:122-123`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L122-L123), [`gestures.cpp:138-146`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L138-L146)
* **Defect:** `g_epoch` is a namespace-static variable initialized to `-1`. `Gestures::Reset()` clears `g`, but does not reset `g_epoch`.
* **Impact:** When running `SkeletonKeyReplay.exe --loop 3` or switching tracking bodies, log timestamps `T(frameMs)` do not restart from `0.0`, producing discontinuous or negative time deltas in session logs.

---

## 2. Guard Stack Analysis & Redundancy

```
  Swipe Event Fires
      │
      ├───────────────────────┬────────────────────────┐
      ▼                       ▼                        ▼
Guard 1: Cooldown       Guard 2: Settle Gate      Guard 5: vLock (Vertical)
 (500 ms hard block)     (1200 ms, speed < 0.91)   (500-1400 ms, |sgVy| < 0.55)
      │                                                │
      ▼                                                ▼
Guard 4: Reversal Guard                         [Strictly Dominates Guard 2]
 (450 ms window)
 [DEAD CODE: 450 ms < 500 ms Cooldown]
```

### 2.1 Dead Code: Guard 4 is Dominated and Masked by Guard 1
* `swipeCooldownMs = 500` ([`config.h:63`](file:///c:/Github/KinectNavigator/src/SkeletonKey/config.h#L63)).
* `revGuardMs = 450` ([`config.h:88`](file:///c:/Github/KinectNavigator/src/SkeletonKey/config.h#L88)).
* When a swipe fires, no new swipe can start until `frameMs >= lastFireMs + 500`.
* The earliest a new swipe can be evaluated is at $\Delta t = 500\text{ ms}$.
* At that point, `frameMs - axLastMs < 450` is **always false**.
* **Verdict:** `revGuardMs` is 100% dead code for all single-swipe sequences. It only ever executed if a user exited an active `RS_HOLD` auto-repeat.

### 2.2 Strict Redundancy: Guard 5 Dominates Guard 2 on Vertical Swipes
* **Guard 2 (Settle):** Window 1200 ms, legacy speed $< 0.91\text{ torso/s}$, 3 leaky frames.
* **Guard 5 (vLock):** Floor 500 ms, ceiling 1400 ms, SG vertical speed $< 0.55\text{ torso/s}$, 4 strict frames.
* For vertical gestures, Guard 5 enforces a tighter velocity threshold, a stricter counter, and a longer failsafe window. Guard 2's settle counter is completely redundant on vertical swipes.

### 2.3 Velocity Stream Schism: The Root of Inferred-Joint False Triggers
The dual-stream architecture runs two velocity estimators with conflicting properties:
1. **Legacy Velocity:** Backward difference on $\alpha = 0.5$ exponential smoother. Has **0 frames lag**, but passes **50% of single-frame jitter spikes** directly into velocity. A $0.3\text{ m}$ inferred snap creates an instant $11\text{ torso/s}$ velocity pulse followed by an artificial reversal on snap-back.
2. **1€ + 5-point Savitzky-Golay (SG):** 2 frames centered lag (~66 ms), but **completely suppresses 1–2 frame jitter spikes**.

| Recognizer Component | Velocity Stream Used | Resulting Behavior |
| :--- | :--- | :--- |
| **Swipe Initiation** | Legacy `evx`, `evy` | Jitter spike instantly triggers `swActive = true`. |
| **Segmentation Split** | Legacy `vel` | Jitter snap-back creates spurious Segment 2. |
| **Horizontal Fire Peak** | Legacy `swSegPeak` | Falsely passes and emits `LEFT`/`RIGHT` on pure jitter. |
| **Vertical Fire Peak** | Legacy + SG `swSegPeakSg` | SG confirms $\approx 0\text{ torso/s}$, so vertical aborts safely. |
| **vLock / Settle** | SG `sgVy` / Legacy `ev` | vLock stays locked on SG; Settle leaks on legacy noise. |

**Verdict:** Horizontal swipes still misfire on jitter because they lack the SG confirmation gate (`swipeVertSgConfirm`) that was added only for vertical swipes.

---

## 3. The Two Unsolved Problems: Kinematic Solutions

### 3.1 Up/Down Oscillation (§3.1)

#### Why time/speed lockouts fail
When scrolling down a list:
$$\text{Stroke 1 (Down)} \longrightarrow \text{Recovery (Up)} \longrightarrow \text{Stroke 2 (Down)}$$
The recovery arm-raise is a ballistic motion ($|v_y| > 2.5\text{ torso/s}$, $\Delta y \approx 0.5\text{ torso}$). Because the user bounces off the bottom to begin the next stroke, the hand never comes to rest at the bottom; any stillness lockout (`vLock`) either forces an unnatural pause or lets the recovery swing leak through.

#### The Physical Discriminators
Human arm kinematics differ between an intentional UP swipe and a recovery UP from a low down-swipe:

```
Intentional UP Swipe                     Recovery UP (from Down-swipe)
====================                     =============================
Starts: Neutral (y ≈ -0.2 to 0.0)        Starts: Deep low (y ≈ -0.6 to -0.8)
Ends:   High overhead (y > +0.20)        Ends:   Neutral chest (y ≈ -0.1 to 0.0)
Hand vs Elbow: Hand ABOVE elbow          Hand vs Elbow: Hand BELOW elbow
               (y_hand - y_elbow > +0.05)               (y_hand - y_elbow < -0.05)
```

```
           [Head]                                  [Head]
             │                                       │
        ┌────┴────┐                             ┌────┴────┐
        │         │                             │         │
     [Elbow]   [Elbow]                       [Elbow]   [Elbow]
        │         ▲                             │         ▲
        │         │ [Hand] (High)               │         │ (Hand trailing below)
     [Hand]       │                             │      [Hand]
                  │ Intentional UP                        │ Recovery UP
               Neutral                                   Deep Low
```

#### Deterministic Invariants:
1. **Vertical Origin / End Zone Gate:**
   - A valid **DOWN** swipe must start at or above neutral ($y_{\text{start}} \ge -0.25$) and end below ($y_{\text{end}} \le -0.40$).
   - A valid **UP** swipe must reach into the upper chest/head zone ($y_{\text{end}} \ge +0.15$). A recovery stroke starting at $y = -0.70$ and ending at $y = -0.05$ is rejected regardless of speed.
2. **Elbow-Relative Pose ($y_{\text{hand}} - y_{\text{elbow}}$):**
   - Kinect v1 elbow tracking is reliable even when wrists jitter. For an intentional UP swipe, the forearm rotates upward such that $y_{\text{hand}} \ge y_{\text{elbow}}$. During a recovery drag, the elbow leads the motion and the hand trails below the elbow ($y_{\text{hand}} < y_{\text{elbow}}$).

---

### 3.2 Wave-Back / Return-to-Park (§3.2)

#### Why the confirm-delay failed
Guard 6 assumed a return-to-park would immediately reverse back outward. When a user finishes navigating and lowers or parks their hand at their chest, **there is no reversal**. The hand simply stops. After 8 frames (266 ms), the timer expires and fires `LEFT` anyway.

#### The Midline Crossing Invariant
For the dominant (right) hand:
* **Neutral Rest Position:** $ex \approx +0.15\text{ to }+0.30$ (on the right side of the chest).
* **Intentional RIGHT Swipe:** Moves outward away from the body ($ex: +0.20 \to +0.60$, centrifugal: $x \cdot v_x > 0$).
* **Return from Right:** Moves inward ($ex: +0.60 \to +0.20$). It stops on its own side of the body ($ex \ge +0.05$).
* **Intentional LEFT (Cross-Body) Swipe:** A deliberate swipe across the body to the left MUST cross the torso midline into negative space ($ex_{\text{end}} \le -0.15$).

```
       LEFT (Cross-Body)        MIDLINE (x=0)        RIGHT (Outward)
    ───────────────────────────┬──────────────┬───────────────────────────
     Target for intentional    │   Neutral    │  Start of deliberate Right
     LEFT swipe (ex < -0.15)   │  (ex ≈ +0.2) │  & Parked position (ex > 0.4)
                               │              │
                               │       ◀──────┼── Return-to-park
                               │   (Stops at  │   (NEVER crosses midline ->
                               │    ex > +0.05│    REJECT as LEFT)
                               │     NO FIRE) │
```

> **Invariant:** A leftward stroke by the right hand is only a valid `LEFT` swipe if it crosses the torso midline ($ex_{\text{end}} < -0.10$).

If a leftward movement terminates at $ex \ge +0.05$, it is geometrically a return-to-rest, not a cross-body swipe. This single positional invariant eliminates return-to-park misfires with zero delay frames.

---

## 4. Refactor vs. Rebuild Architecture Verdict

### 4.1 Assessment: Why Full $P+$ is Overkill
Pass 3 recommended a `$P+` point-cloud template matcher. However:
1. **Vocabulary is 4 Cardinal Directions:** For Up, Down, Left, and Right, the trajectory is a 1D vector. An $\text{atan2}(\Delta y, \Delta x)$ direction check plus net displacement classifies a segmented stroke with 100% mathematical fidelity. Template point-clouds add unnecessary matching overhead for straight lines.
2. **Confirm and Back are Held Poses:** Confirm (hand raised) and Back (arm 45° down-and-out) are static dwell poses, not strokes. Point-cloud matchers do not apply to static holds.
3. **The Core Value is the Segmenter:** The key breakthrough is **Energy-Bounded Stroke Segmentation** driven by the 1€ + SG clean signal.

### 4.2 The Target Architecture: "Minimal Energy Segmenter"

```
 Raw Skeleton Joints (SC, HC, Hands, Elbows)
                    │
                    ▼
     1€ Adaptive Filter (MinCutoff 1.0Hz, Beta 0.05)
                    │
                    ▼
   5-point Savitzky-Golay Differentiator (sgVx, sgVy)
                    │
       ┌────────────┴────────────┐
       ▼                         ▼
Static Pose Engine         Stroke Segmenter (Kinetic Energy)
(Confirm & Back Dwell)     • Open:   |v_sg| > 1.2 torso/s
                           • Track:  Traj(x, y), Peak(v), Start(x, y)
                           • Close:  |v_sg| < 0.4 torso/s (Zero-Crossing)
                                 │
                                 ▼
                         Geometric Classifier
                         • Direction: atan2(Δy, Δx)
                         • Net Travel: |Δpos| ≥ 0.25 torso
                         • Invariants:
                           - Left:  ex_end < -0.10 (Midline cross)
                           - Right: ex_end > +0.35 (Outward extension)
                           - Up:    ey_end > +0.15 && y_hand > y_elbow
                           - Down:  ey_start ≥ -0.25 && ey_end ≤ -0.40
                                 │
                                 ▼
                          Output Action
```

---

## 5. Concrete Migration Order

```
Step 1: Commit to 1€+SG Velocity
  • Delete legacy backward-difference calculation in Smooth().
  • Route all gesture logic, dwell stillness, and swipe thresholds exclusively to sgVx/sgVy.
  • Fix the mirror bug on sgVx.
  └─► Verify: Synthetic tests PASS, 9 m/s spikes gone from logs.

Step 2: Add Positional Invariants (Kill Oscillation & Wave-Back)
  • Add Midline Crossing check for Left swipe (ex_end < -0.10).
  • Add Upper Zone (ey_end > 0.15) & Elbow check (y_hand > y_elbow) for Up swipe.
  • Add Origin check (ey_start ≥ -0.25) for Down swipe.
  └─► Verify: Replay skcap captures; oscillation and wave-back false fires vanish.

Step 3: Prune the Guard Stack
  • Remove swipeCooldownMs (500 ms) and dead revGuardMs.
  • Remove swipeConfirmFrames delay (no longer needed with midline invariant).
  • Consolidate vLock and SettleGate into a single post-stroke stillness check.
  └─► Verify: Responsive consecutive same-direction swipes without latency.

Step 4: Consolidate Config Knobs
  • Reduce Config struct from 71 constants to ~22 clean, documented knobs.
```

---

## 6. Config Knob Pruning Table (71 $\to$ ~22)

| Knobs to Remove | Rationale |
| :--- | :--- |
| `swipeConfirmFrames`, `swipeReturnBand` | Obsoleted by the Midline Crossing invariant ($ex_{\text{end}} < -0.10$). |
| `revGuardMs`, `revGuardPeakK`, `revGuardDistK` | Obsoleted; dead code masked by 500 ms cooldown. |
| `vLockMinMs`, `vLockSettleVel`, `vLockSettleFrames`, `vLockMaxMs` | Replaced by single unified SG post-stroke energy gate. |
| `swipeVertSgConfirm` | Redundant once SG velocity is used across the entire recogniser. |
| `smoothFast`, `smoothSlow` | Replaced by 1€ filter parameters (`filter1eMinCutoff`, `filter1eBeta`). |
| `swipeVxCeiling` | Unnecessary; 5-point SG naturally eliminates single-frame velocity spikes. |

---

## 7. The Single Highest-Leverage Change Right Now

**Commit the entire recogniser to the 1€ + Savitzky-Golay velocity stream and apply the Midline Crossing + Vertical Zone invariants.**

1. **Fix `sgVx` mirroring** ([`gestures.cpp:458, 480`](file:///c:/Github/KinectNavigator/src/SkeletonKey/gestures.cpp#L458)):
   ```cpp
   const float sgVxEffective = (c.mirror ? -1.0f : 1.0f) * g.sgVx;
   ```
2. **Enforce Midline Crossing on Left Swipes**:
   ```cpp
   if (action == GestureAction::Left && ex > -0.10f) return GestureAction::None;
   ```
3. **Enforce Upper Bound on UP Swipes**:
   ```cpp
   if (action == GestureAction::Up && (ey < 0.15f || dh.y < nav.SkeletonPositions[domElbow].y)) return GestureAction::None;
   ```
