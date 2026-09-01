# Research brief — "How should a hands-free gesture recogniser actually model human movement?"

**For:** Gemini Deep Research (3rd pass on the Skeleton Key project).
**Written:** 2026-09-01.
**Why a 3rd pass:** the first two passes (see "What prior research already covered" below)
answered *what gestures* and *what threshold numbers*. Both produced usable framing but the
numbers were uncited engineering defaults and neither engaged with the **algorithmic and
motor-control layer**: *how* a recogniser should be structured, and what the science of
natural human movement says about the signal it will actually see. Months of hand-tuning
thresholds on a finite-state machine have hit diminishing returns and keep producing
whack-a-mole regressions. We want to step back and design the recognition logic from first
principles.

---

## 1. What we are building

**Skeleton Key** — a hands-free menu-navigation layer for a dance video game (menus only,
never gameplay). A user standing in their living room drives the game's menus with arm
gestures instead of a keyboard or phone. The recogniser watches a skeleton stream and
synthesises keystrokes:

| Command | Key | Notes |
|---|---|---|
| Navigate Left / Right / Up / Down | arrow keys | discrete step; also needs a "hold to keep scrolling" auto-repeat for long lists |
| Confirm / Select | Enter | once per screen, low frequency |
| Back / Cancel | Esc | once per screen, low frequency |

~6 commands total. One developer, no data-collection or model-training pipeline, the
recogniser has to run in a tiny always-loaded library at 30 Hz with a few-millisecond
budget, and every threshold has to stay human-readable and editable in a config file
because tuning happens by hand against recorded sessions.

**Audience:** the developer plus a handful of friends in the game's modding community,
playing in ordinary living rooms at whatever distance and clutter they happen to have.

---

## 2. Hard sensor / signal constraints (these do not change)

- **Microsoft Kinect v1** (structured-light depth + RGB), 20-joint skeleton at a nominal
  **30 Hz**. This is the only sensor. No RGB/depth image processing is on the table — skeleton
  joints only.
- **Depth (toward/away from sensor) is unusable** on distal joints — too noisy. All
  reasoning is in the **frontal plane only** (left/right, up/down). "Push to click" is
  permanently ruled out.
- **The hands are usually reported "inferred", not "tracked"** — whenever a hand crosses
  the torso silhouette, moves fast, or is partly occluded. Inferred hand position carries
  roughly **±0.1–0.3 m of jitter** and can spike to superhuman velocities frame-to-frame.
- **No finger data, no hand open/closed state, no wrist orientation.**
- **Rock-solid joints:** shoulder-centre, spine-base, head. **Moderate:** elbows.
  **Worst:** hands/wrists. We normalise everything by torso length (shoulder-centre to
  spine-base distance, ~0.40 m) so it is stature-independent; distances below are quoted in
  "torso units".
- **Real deployment distance is ~2 m in a cluttered room** — worst-case for a v1 skeleton,
  which prefers ~2.5–4 m. Assume the pessimistic noise floor.
- Tracking IDs churn on every re-acquire; the skeleton can drop for a second or two and
  come back.
- Latency budget: this is a menu, not a cursor. A few hundred ms of recognition latency is
  acceptable if it buys reliability. We do **not** know how much — quantifying the
  perceptual latency tolerance for discrete mid-air menu commands is one of the questions.

---

## 3. Our current approach, and why we think it is wrong-headed

The recogniser is a hand-built **finite-state machine over smoothed joint offsets**:
dual-rate exponential smoothing on each hand, per-axis velocity from frame timestamps, then
a pile of thresholds — swipe velocity, swipe distance, an axis-dominance ratio, a "ballistic
peak" gate, a settle counter, cooldown windows, a segment counter that splits a stroke at
velocity reversals, dwell timers for Confirm/Back, an "arm fully extended and roughly
horizontal" test for auto-repeat. Roughly 40 tunable constants.

**Symptoms that make us think the model, not the tuning, is the problem:**

- Every fix trades one failure for another. Tightening the swipe gate to stop a
  return-stroke firing a backwards swipe made cold swipes from rest fail. Adding a
  windup-tolerance window to absorb a pre-stroke counter-motion made normal swipes feel
  laggy and was reverted. Splitting the stroke into segments at reversals helped the
  counter-motion case but jitter now spuriously bumps the segment count.
- The user's natural navigation motion is a **wave**, not a crisp **swipe** — a wrist-led,
  elbow-tucked, low-amplitude oscillation. The velocity/distance swipe model barely sees it.
  A whole-arm reach registers well but is tiring within a minute.
- A pre-movement **counter-motion** ("like countersteering a bike — the hand goes the wrong
  way a little before the real stroke") keeps firing the wrong direction or aborting the
  real stroke. We suspect this is a real, named motor phenomenon we should be modelling, not
  filtering out.
- Commands **collide in feature space**: a dominant-hand cross-body Confirm swipe looks
  identical to "navigate left"; lowering the arms to rest fires "navigate down"; the game's
  own dance choreography satisfies a static "engage" pose. We keep discovering these
  collisions live and patching them individually.
- Distinguishing the **stroke** of a gesture from its **retraction / return to rest** is
  done with ad-hoc cooldowns and settle counters and is fragile.

We think we are re-deriving, badly and piecemeal, things that motor-control science and the
gesture-recognition literature already know.

---

## 4. What prior research already covered (do **not** just repeat this)

- **Pass 1 — "Kinect Gesture Navigation UI Design".** Biomechanics of mid-air interaction:
  Consumed Endurance / "gorilla arm" fatigue, why Dance Central used swipes not dwell, PhIZ
  / skeletal anchoring, a body-anchored dead-zone + activation-band positional navigation
  model, engagement dwell, auto-repeat rate. Gave a parameter table (dead zone ±10 cm,
  activation ±15 cm, swipe > 1.2 m/s / > 0.3 m, engage dwell 600 ms, repeat 3 Hz, cooldown
  500 ms) — but **none of it traced to a measured source**, and the table's own jitter
  figure (±10–30 cm) is *larger* than the bands it proposes.
- **Pass 2 — "Kinect Gesture Design and Segmentation".** A ranked Back-gesture shortlist
  (winner: contralateral downward slash — dominant hand diagonally down-and-across toward
  the opposite hip). A "quiescence precondition gate": weighted whole-body kinetic energy +
  hip-centre velocity below a floor, sustained ~300 ms, before an engage pose is even
  considered, so a dancing body can't engage. Prior art: Vogel–Balakrishnan 3-state clutch,
  Xbox 360 Guide gesture, MS Kinect HIG radial-fill feedback. Again, most numbers were
  trapped in formula images and provisional.

**What neither pass did, and this pass should:** treat the recogniser as a **signal-
processing + pattern-recognition problem** and ground it in the **motor-control science of
how people actually produce these movements**.

---

## 5. Research questions

### A. Motor-control science of a voluntary arm gesture

1. **Gesture phases.** The gesture-studies literature (Kendon, McNeill) decomposes a
   gesture unit into preparation → (pre-stroke hold) → stroke → (post-stroke hold) →
   retraction. How well-established is this, quantitatively? What are typical durations and
   kinematic signatures of each phase for a mid-air arm gesture, and how should a recogniser
   use the phase structure — e.g. is the correct design to **detect the stroke and
   explicitly model-and-ignore the preparation and retraction**, rather than our current
   cooldown hacks?
2. **Ballistic vs. controlled movement.** Fast aimed arm movements are largely feedforward
   ("ballistic") with a characteristic **bell-shaped, often asymmetric velocity profile**
   (accel phase shorter than decel). Slow/corrective movements are feedback-driven and look
   different. How reliably can the two be told apart from a 30 Hz frontal-plane wrist
   trajectory, and does that distinction map cleanly onto "deliberate command" vs.
   "incidental motion"?
3. **The pre-movement counter-motion.** The user reports a small opposite-direction motion
   just before the real stroke. Is this **anticipatory postural adjustment / counter-
   rotation / a wind-up / pre-tension**? Is it a known, characterised phenomenon with a
   typical amplitude and lead time? What is the right way to handle it — subtract it,
   measure the stroke from the reversal point, wait it out, or use it as a *predictive cue*
   that a stroke is coming?
4. **Minimum-jerk / two-thirds power law.** Do the standard trajectory-smoothness models
   (minimum-jerk trajectory, the two-thirds power law relating curvature and speed) give a
   usable *prior* for "this trajectory was produced by a human on purpose" vs. sensor noise
   or an incidental bump?
5. **Fitts' law / target acquisition.** There is no on-screen cursor here, but menu
   navigation is still target acquisition (N items in a list). Does Fitts-style analysis say
   anything about whether discrete-step, rate-controlled (velocity), or position-controlled
   (absolute) navigation will feel best for lists of, say, 5–50 items?
6. **Fatigue over a session.** Consumed Endurance covers static hold fatigue. What does the
   literature say about how gesture **kinematics drift** as a user tires over a 20–40 minute
   session — amplitude shrink, speed drop, more inferred frames — and should the recogniser
   adapt thresholds online to compensate?

### B. Between-person and within-session variation

7. How much do healthy adults vary in gesture **amplitude, peak speed, and duration** for
   "the same" command? Is there a standard way to express a threshold as a fraction of a
   short per-user calibration or running baseline instead of an absolute number?
8. **Wave vs. swipe.** Our user's natural navigation gesture is a low-amplitude wrist/
   forearm **wave**, not a whole-arm swipe. Is there research on the kinematic difference,
   and on which is less fatiguing / more repeatable / more reliably tracked? Should the
   recogniser be built around an **oscillatory** primitive (detect a directional
   half-cycle) rather than a **ballistic stroke** primitive?
9. **Handedness** and mirrored/seated users — how much does this actually perturb the
   frontal-plane signal, and is auto-detecting the active hand from early motion a solved
   problem?

### C. Recognition architecture — the core question

10. Give a clear-eyed comparison of the practical options for a **small, fixed vocabulary
    (~6), no training data, real-time, hand-tunable** recogniser:
    - hand-built FSM + thresholds (what we have);
    - **template / instance methods** — $1 / $P / $Q / Protractor family, DTW against a few
      recorded exemplars;
    - **probabilistic sequence models** — HMM, CRF, and whether these are worth it without a
      labelled corpus;
    - **lightweight windowed-feature classifiers** (decision tree / SVM / tiny MLP on
      hand-crafted features) and how much data they really need;
    - **segmentation-first** pipelines — segment the stream at motion boundaries (velocity
      zero-crossings, motion-energy troughs, "rest state" detection) and classify each
      segment.
    For each: data requirements, real-time cost, tunability/debuggability by one developer,
    robustness to the ±0.1–0.3 m inferred-hand jitter, and how gracefully it fails.
11. What have **shipping full-body gesture UIs** actually used — Kinect-era Xbox dashboard
    and games, current living-room TV gesture UIs, VR system menus (which face the same
    "no physical support, open-loop, Midas-touch" problems)? What converged as best practice
    and what was abandoned?
12. Is there a defensible **hybrid** — e.g. a lightweight always-on segmenter feeding a
    template matcher, with an FSM only for the engage/disengage clutch? If you were
    architecting this from scratch in 2026 for exactly these constraints, what would you
    build?

### D. Continuous segmentation / gesture spotting

13. The **gesture spotting** problem — finding start/end of meaningful gestures in a
    continuous stream that is mostly non-gesture. What are the established approaches
    (explicit rest-state / neutral models, threshold models / garbage HMMs, motion-energy
    gating, sliding-window + non-maximum suppression)? Which suit our constraints?
14. **Sub-gesture and retraction rejection.** How does the literature stop (a) the retraction
    stroke of a gesture, and (b) a partial/aborted gesture, from being recognised as a
    command in their own right? This is one of our recurring bugs.
15. **The Midas touch problem** for full-body input — the state of the art in clutch /
    engagement mechanisms beyond "hold a pose for N ms", and how to make engagement both
    hard to trigger accidentally (dancing user) and not annoying (deliberate user).

### E. Signal conditioning

16. Best-practice filtering for a **jittery 30 Hz joint stream** feeding a real-time
    interaction: **1€ filter** vs. Kalman vs. double-exponential vs. moving average —
    concretely, for this jitter profile and latency budget. Filter **position, velocity, or
    both**? How to handle the "inferred → tracked" transition and velocity spikes from
    inferred frames without adding lag to real fast strokes.
17. Deriving clean **velocity/acceleration** at 30 Hz from noisy position — central
    differences, Savitzky–Golay, filtered differentiators — with the accuracy/latency
    trade-off quantified.

### F. Feedback — closing the loop on an open-loop input

18. With no cursor, the only feedback is the menu reacting and whatever overlay/audio we
    add. What does the motor-learning / HCI literature say is the **minimum feedback**
    needed to make discrete mid-air commands feel controllable and learnable — visual
    (progress fills, direction hints), audio (per-command earcons), timing? How much does
    good feedback relax the accuracy requirement on the recogniser itself?

### G. Vocabulary design for non-confusability

19. Is there a principled method to choose a set of ~6 full-body commands that are
    **mutually separable in the sensor's feature space**, each **low-fatigue**, and each a
    **natural mapping**, given our joint set and noise? We keep finding collisions by
    accident (cross-body Confirm ≡ nav-left; arm-drop ≡ nav-down; dance pose ≡ engage). Is
    there a design procedure or checklist that would have caught these on paper?
20. Given everything above: **what specific 6-command vocabulary would you recommend**, with
    the kinematic definition and detection cue for each, chosen so they cannot collide?

---

## 6. Deliverables we want from the report

1. A **recommended recognition architecture** for these exact constraints, with the
   reasoning, and a **phased migration path** from our current FSM (what to keep, what to
   replace first, how to validate each step against recorded sessions).
2. A **gesture-phase-aware processing model** — how to represent preparation / stroke /
   retraction / rest and use that structure instead of cooldown hacks.
3. A concrete **filtering + differentiation recommendation** for the 30 Hz jittery stream.
4. A **segmentation / spotting** recommendation (how to find gesture boundaries and reject
   retractions and partials).
5. A recommended **6-command vocabulary** with per-command kinematic definitions and a
   non-collision argument.
6. A **feedback spec** (visual + audio) tied to what it buys in recogniser tolerance.
7. Every quantitative claim: **cite a primary source** (HCI venues — CHI, UIST, TEI, GI;
   motor-control / motor-behaviour journals; the gesture-recognition literature). Where a
   number is an engineering convention rather than a measured result, **say so explicitly**.
   Do not present a parameter table as "empirically validated" unless each row has a
   citation. Prefer fewer, well-sourced numbers over a complete-looking table of guesses.
8. Call out where our stated constraints (30 Hz, frontal plane only, inferred hands,
   no training data, ~2 m) make a technique from the literature **not applicable**, rather
   than recommending it anyway.

## 7. Explicitly out of scope

Hardware other than Kinect v1; RGB/depth image processing; anything needing a labelled
training corpus we would have to collect at scale; finger/hand-state/wrist-orientation
input; depth-axis ("push") gestures; on-screen cursor control.
