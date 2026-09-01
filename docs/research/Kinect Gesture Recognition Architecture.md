# Research Report: First-Principles Modeling of Hands-Free Gesture Recognition for Cluttered Environments

## Introduction and System Constraints

The design of a reliable, real-time gesture recognition architecture for mid-air, open-loop interaction is a notoriously difficult problem in human-computer interaction. When constrained to a legacy depth sensor like the Microsoft Kinect v1 operating at a nominal 30 Hz in cluttered living room environments, the challenge transitions from standard computer vision to rigorous signal processing and motor-control modeling. The current architecture for the "Skeleton Key" project—a manually tuned finite-state machine operating over dual-rate exponentially smoothed joint offsets—has reached an engineering asymptote. The "whack-a-mole" regression phenomenon observed in this finite-state machine is a direct consequence of treating human kinematics as a sequence of boolean threshold gates, rather than continuous biomechanical phenomena.

To achieve high reliability, low perceptual latency, and immunity to severe sensor jitter (the  ±0.1  to  0.3  meter noise floor of "inferred" joint states), the recognizer must be rebuilt from first principles. It must mathematically model how the central nervous system plans and executes voluntary arm movements, computationally separate intentional strokes from incidental body noise, and classify trajectories using scale-invariant template matching. Crucially, the constraints of this project—no massive training data pipeline, a single developer, tuning via config files, frontal-plane-only tracking, and no explicit finger or hand-state data—render many state-of-the-art machine learning approaches entirely inapplicable.

This report provides a comprehensive, exhaustive analysis of the algorithmic and motor-control layers of mid-air gesture recognition. It dissects the biomechanics of gesture phases, provides concrete mathematical recommendations for 30 Hz signal conditioning, evaluates modern lightweight recognition architectures, and culminates in a non-confusable, low-fatigue 6-command vocabulary specifically engineered for the noise floor of the Kinect v1 sensor.

## The Biomechanics of Voluntary Gestures

The failure of positional finite-state machines often stems from a fundamental misunderstanding of how the human body moves. Hand-coded positional thresholds implicitly assume that human movement is rigid, linear, and perfectly bounded. In reality, biological motion is governed by specific kinematic laws, multi-joint coordination, and deep neurological planning mechanisms. To build a resilient recognizer, the system must filter for the kinematic signature of human intent.

### The Phase Structure of a Gesture Phrase

The gesture-studies literature, pioneered by Kendon and McNeill, unequivocally establishes that a natural human gesture is not a single continuous motion, but a highly structured "gesture phrase" decomposed into distinct, sequential phases1. For a mid-air interface, understanding these phases is the key to separating a command from a repositioning movement.

The standard gesture unit is divided into five phases2:

- Preparation Phase: The limb moves from a resting state into a position of readiness. This phase is typically slower, highly variable in trajectory, and lacks distinct acceleration peaks. Its primary function is physiological positioning, carrying no semantic meaning.
- Pre-Stroke Hold: A brief cessation of motion, often lasting 50 to 200 milliseconds, where the central nervous system finalizes the motor program for the accented movement.
- Stroke: The nucleus of the gesture. The stroke carries the semantic meaning (or in this context, the system command). It is characterized by high kinetic energy, focused physical effort, and a rapid, highly stereotyped velocity profile.
- Post-Stroke Hold: A temporary cessation of motion at the apex of the stroke, serving as a boundary marker before the arm relaxes.
- Retraction (Return): The passive, gravity-assisted return of the limb to a rest state. Retractions are often kinematically sloppy, highly variable, and require minimal muscular engagement.
The structural reality of the gesture phrase dictates how a recognizer should be designed. The current finite-state machine uses ad-hoc cooldown hacks to ignore retractions, which is a fundamentally flawed strategy. A robust recognizer must explicitly identify the stroke phase based on its unique kinetic signature and actively discard the preparation and retraction phases. Because retractions and preparations lack the focused kinetic energy and stereotyped minimum-jerk trajectory of a stroke, continuous segmentation models must use velocity-peak bounding to isolate the stroke nucleus before any directional classification occurs2.

### Ballistic Kinematics and the Minimum-Jerk Prior

Fast, aimed mid-air arm movements—such as user interface swipes or directional waves—are executed as feedforward, "ballistic" motor programs. Because mid-air gestures lack a physical surface to provide haptic feedback, the central nervous system plans the entire trajectory in advance5.

The most critical distinguishing feature of a ballistic stroke is its velocity profile. Unlike the constant-velocity assumption of basic spatial thresholds, human ballistic strokes follow a bell-shaped velocity curve7. Furthermore, these profiles are frequently asymmetric, meaning the acceleration phase (from rest to peak velocity) does not equal the deceleration phase (from peak velocity to rest). Target acquisition tasks typically exhibit a shorter, sharper acceleration phase followed by a prolonged deceleration phase as the nervous system relies on visual feedback to hone in on the target5.

This movement is governed mathematically by the Minimum-Jerk Principle and the Two-Thirds Power Law11.

The Minimum-Jerk Principle asserts that the central nervous system plans trajectories that minimize the time integral of the square of jerk, which is the third derivative of position14. The mathematical optimization of this cost function results in maximally smooth paths. In tandem, the Two-Thirds Power Law, an empirical law formulated by Lacquaniti, Viviani, and Terzuolo, states that the angular velocity of a limb is proportional to the two-thirds power of its path curvature11. Expressed formally, the tangential velocity  v(t)  and curvature  κ(t)  relate such that  v(t) = c·κ(t)^(−1/3) . In practical terms, human hands slow down predictably when navigating sharp curves and speed up predictably on straightaways12.

These kinematic laws provide a profound mathematical prior for distinguishing intentional commands from incidental body noise, "dancing," or sensor jitter. Sensor noise, which is generally random or Gaussian, inherently fails to exhibit a high correlation between log-velocity and log-curvature11. Similarly, incidental bumps do not minimize jerk. Rather than attempting to filter out jitter with heavier absolute positional thresholds, the recognizer can evaluate the mathematical smoothness of an extracted segment. If the trajectory profile sharply violates minimum-jerk expectations, it is overwhelmingly likely to be an artifact of the Kinect's "inferred" state noise and should be immediately rejected as non-intentional.

### The Pre-Movement Counter-Motion (Anticipatory Postural Adjustments)

A recurring symptom noted in the current finite-state machine is the user executing a "counter-motion"—moving the hand in the opposite direction slightly before the primary stroke—which triggers false positives in the opposite direction.

In motor-control science, this is not an anomaly but a well-documented biological necessity known as an Anticipatory Postural Adjustment (APA)18. When a human initiates a rapid focal movement, such as an arm swipe, the acceleration of the limb induces a self-inflicted mechanical perturbation to the body's overall center of mass19. To prevent a loss of balance, the nervous system pre-activates postural muscles, typically in the legs and trunk, 50 to 150 milliseconds before the prime mover muscles of the arm activate21.

Crucially, APAs can manifest kinematically in the focal limb itself. To generate the necessary dynamics and leverage the stretch-shortening cycle of the muscles, users will instinctively execute a "wind-up" or counter-movement18. Bouisset and colleagues demonstrated that these counter-movements are highly dependent on the phase of the body's posture and the required velocity of the stroke. The direction of the APA can even reverse entirely depending on whether the user is leaning slightly forward or backward18.

Attempting to handle APAs with an explicit "windup-tolerance window" or a reversal segmenter is a losing battle because the APA is fundamentally dependent on the user's instantaneous center of gravity, which the 20-joint Kinect skeleton cannot calculate accurately.

- Do not use it as a predictive cue because the direction of the APA is dynamically assigned by the brain to maintain balance and can invert18.
- Do not trigger on it. The FSM fires backwards because it relies on instantaneous velocity thresholds that are easily breached by a sharp wind-up.
- The Solution: Advanced template-matching algorithms naturally absorb this phenomenon. If a user consistently performs an APA, a template matcher simply aligns the "hook" at the start of the trajectory to the recorded template using dynamic time warping or point-cloud matching. Alternatively, an energy-bounded continuous segmenter will view the APA as part of the pre-stroke kinetic buildup, isolating the true high-energy stroke starting from the velocity zero-crossing immediately following the APA.

### Fitts' Law and Target Acquisition in Open-Loop Menus

While Skeleton Key operates without a visible on-screen cursor, driving a menu is still fundamentally a target acquisition task. The user is attempting to navigate through a list of  N  items.

Fitts' Law dictates that the time required to rapidly move to a target area is a function of the ratio between the distance to the target and the width of the target. In mid-air, open-loop interfaces, the lack of haptic feedback and the absence of a continuously tracked cursor fundamentally break absolute position control. If the system maps the absolute height of the user's hand to the position in a list, the user experiences massive overshoot because they lack the proprioceptive precision to land exactly on "item 14 of 50" in mid-air.

Consequently, the literature indicates that discrete-step (event-driven) or rate-controlled (velocity-driven) navigation is vastly superior for list navigation23. For short lists (under 10 items), discrete strokes parsed as distinct commands provide the highest accuracy and satisfaction. For long lists (up to 50 items), a rate-controlled "hold to scroll" mechanism—where the displacement of the hand from an anchored neutral zone dictates the velocity of the scroll—minimizes the cognitive load and physical distance required to navigate24. Absolute positioning should remain strictly out of scope.

## Addressing Inter- and Intra-User Variations

### Wave vs. Swipe: Oscillatory vs. Ballistic Primitives

The user’s natural inclination is a low-amplitude wrist or forearm "wave" rather than a whole-arm "swipe."

The biomechanics literature evaluates the fatigue of mid-air interactions using the Consumed Endurance (CE) metric, initially proposed by Hincapié-Ramos et al.25. Consumed Endurance quantifies the ratio of the active interaction time to the maximum physiological endurance time for a specific arm posture. Whole-arm swipes require sustained elevation of the upper arm, heavily taxing the deltoid and rotator cuff muscles. This leads to rapid "gorilla arm" fatigue, yielding an unsustainably high Consumed Endurance25. Conversely, a wrist-led wave with the elbow tucked to the torso requires minimal shoulder torque, yielding a vastly lower CE.

However, the hardware constraints must supersede the biomechanical ideal. The Microsoft Kinect v1, operating at a 2-meter deployment distance in a cluttered room, operates at the absolute limits of its optical and structured-light resolution. Distal joints (wrists and hands) exhibit extreme tracking churn, spatial collapsing, and noise because the sensor lacks the physical resolution to distinguish the hand from the forearm when the elbow is tucked close to the torso.

A pure wrist-wave is essentially invisible below the Kinect v1 noise floor. A whole-arm swipe is too fatiguing. The necessary compromise is a forearm-driven stroke. The user loosely anchors the elbow at their side and pivots the forearm across the sagittal or transverse planes. This generates a displacement of approximately 0.2 to 0.3 torso units, surpassing the sensor's  ±0.1 m noise floor, while keeping Consumed Endurance manageable over a 40-minute session.

### Fatigue, Kinematic Drift, and Handedness

Over a 20 to 40 minute dance session, users will fatigue. As fatigue sets in, kinematic drift occurs: the amplitude of the gestures shrinks, peak velocities drop, and the user's posture slumps, moving joints closer to the body where they are more likely to be classified as "inferred" due to silhouette occlusion27.

Expressing thresholds as absolute metric numbers (e.g.,  1.2  m/s) guarantees failure as the user tires. Instead, the recognizer should normalize all spatial input by the user's immediate torso length (the vector distance between the shoulder-center and spine-base). Furthermore, to account for shrinking amplitude, the continuous segmenter should dynamically lower its kinetic energy gating thresholds based on a rolling average of the last 10 successful commands.

Regarding handedness and mirrored users, auto-detecting the active hand from early motion in a dancing scenario is computationally risky and prone to false positives. Because the user is engaged in full-body movement, both hands will exhibit high kinetic energy. The standard practice in shipping titles (e.g., Xbox 360 Kinect dashboard) is to lock active handedness based on whichever hand initiates the explicit engagement "clutch" gesture, ignoring the opposite hand entirely until the clutch is released and re-engaged28.

## Signal Conditioning for 30 Hz High-Jitter Streams

The Kinect v1 skeletal stream is fundamentally noisy. When hands occlude the torso or move rapidly, the proprietary tracking algorithms shift the joint confidence from "tracked" to "inferred." The inferred state relies on an internal kinematic solver that frequently snaps, generating high-frequency jitter of  ±0.1  to  0.3  meters.

A naive two-frame central difference velocity calculation on an inferred frame can yield a physically impossible artifact. If the hand snaps  0.3  meters in a single  33  millisecond frame, the calculated velocity is  9.0  m/s. This instantly shatters finite-state machine thresholds, explaining the spurious segment splits and direction misfires. Dual-rate exponential smoothing is inadequate because it inextricably links spatial smoothing to temporal lag. Heavy smoothing stops the jitter but introduces massive phase delay, making fast strokes feel sluggish and unresponsive.

### Positional Filtering: The 1 Euro Filter

The definitive standard for conditioning noisy, real-time biological kinematics for interactive systems is the 1 Euro Filter (1€ Filter), introduced by Casiez et al.29. It is an adaptive, first-order low-pass filter that dynamically computes its cutoff frequency based on the signal's rate of change.

The 1 Euro Filter is remarkably lightweight and requires only two tunable parameters:

-  `f_cmin`  (Minimum Cutoff Frequency): Controls the filtering at very low speeds. Setting this to a very low value (e.g.,  1.0  Hz) heavily smooths the signal when the hand is resting or moving slowly, effectively obliterating the "dancing" jitter29.
-  β  (Speed Coefficient): Controls the cutoff frequency scaling at high speeds. As the hand accelerates during a stroke, the filter dynamically opens the cutoff frequency. This reduces phase lag to near zero during the ballistic phase, allowing the true peak of the stroke to pass through unattenuated29.
Other advanced filters, such as the Kalman filter, perform poorly in this specific constraint profile. A standard Kalman filter expects Gaussian measurement noise; the snapping behavior of an "inferred" joint transition is highly non-Gaussian and causes the Kalman state estimate to wildly overshoot. The 1 Euro Filter's heuristic approach is vastly superior for human-computer interaction contexts.

### Kinematic Differentiation: Savitzky-Golay

Calculating clean velocity and acceleration at 30 Hz from a noisy position stream is a classic ill-posed problem. Simple central difference methods amplify high-frequency noise because the derivative operator acts as a high-pass filter.

To safely derive velocity for the purposes of segmentation, the Savitzky-Golay (S-G) filter is the optimal mathematical approach32. Instead of computing the raw slope between two adjacent points, an S-G filter fits a low-degree polynomial (typically 2nd or 3rd order) to a sliding window of points using linear least squares, and then analytically differentiates the polynomial to find the velocity32. This effectively acts as a low-pass differentiator.

For a 30 Hz stream, an S-G window size of 5 frames (approximately 166 milliseconds) is the ideal balance. It is wide enough to mathematically bridge a 1 to 2 frame "inferred" spike without transferring a  9.0  m/s artifact into the velocity stream. Simultaneously, a 5-frame window satisfies the perceptual latency budget, adding less than 100 milliseconds of group delay to the pipeline.

## Continuous Segmentation and Gesture Spotting

The hardest problem in continuous gesture recognition is the Gesture Spotting Problem: identifying the precise start and end of a meaningful stroke within a continuous stream of non-gesture movements35. The current finite-state machine struggles with accidental activation and retraction misclassification precisely because it attempts to use static positional regions and arbitrary timers to guess when a gesture starts and ends2.

### Rejecting Retractions via Energy Gating

Retractions are the passive return of the limb to rest1. In a purely spatial feature space, a retraction looks exactly like a reverse command. For example, pulling the arm back to the body after a "Navigate Right" stroke generates a leftward trajectory that the recognizer interprets as "Navigate Left."

To reject retractions and partial sub-gestures without using fragile cooldown timers, the system must utilize a kinetic energy gate. Rather than tracking spatial boundaries, the segmenter calculates the scalar velocity magnitude of the active wrist (derived via the Savitzky-Golay filter). A stroke is structurally bounded by distinct velocity minima (zero-crossings)2.

The logic flows as follows: The system waits for a state of near-zero velocity. Once the velocity exceeds a dynamic noise floor threshold, a segment is opened. The system records the trajectory until the velocity drops below the noise floor again (the post-stroke hold), at which point the segment is closed and passed to the classifier. Because a retraction happens after this segment is closed, the system can simply ignore the subsequent segment if it occurs too rapidly, or rely on the fact that retractions lack the minimum-jerk acceleration profile of an intentional stroke and will score poorly in a template match.

### The "Clutch": Managing the Midas Touch

The Midas Touch problem refers to the system inadvertently interpreting every scratch, stretch, or dance move as a command. Because the user is actively dancing, the recognizer must utilize an explicit Clutch mechanism to engage and disengage the menu logic28.

A standard "hold a pose for  N  milliseconds" clutch is prone to false positives during dance choreography that happens to briefly match the pose. A superior, highly robust approach combines a Quiescence Precondition with an Anatomical Contralateral Anchor.

- Quiescence: The specific hand used for commanding must exhibit total kinetic energy below a strict threshold (stabilized by the 1 Euro filter) for an uninterrupted duration. The engineering convention for this dwell time in living-room interfaces is 300 to 500 milliseconds.
- Anchor: The non-dominant hand must be placed in a specific, anatomically anchored location that is highly unnatural during dance—such as resting the wrist on the hip (spine-base joint). Measuring the wrist position relative to the spine-base is skeletal anchoring; it is perfectly robust to bounding box scaling, camera distance, and user stature.
When the non-dominant hand satisfies the anchor condition and the dominant hand satisfies the quiescence condition, the clutch is engaged. When the anchor is released, all tracking is immediately discarded.

## Recognition Architecture Analysis and Recommendation

Given the strict constraints—a fixed vocabulary of approximately 6 commands, no labeled training corpus, 30 Hz inferred joints, real-time execution limits, and manual tunability by a single developer—we must rigorously evaluate the state of the art in classification architectures.

|
Architecture
 |
Description
 |
Viability for Constraints
 |
Reasoning
 |
|
Heuristic FSM
 |
Thresholds on velocity, distance, and axis-dominance.
 |
Poor
 |
Highly fragile. Explodes in state complexity when handling edge cases. Fails on APAs and retractions. Hard to maintain.
 |
|
Probabilistic Sequence Models
 |
HMM, CRF treating gesture as a sequence of states.
 |
Disqualified
 |
Excellent for temporal warping, but entirely dependent on a massive, labeled training corpus to estimate transition probabilities. Unfeasible for a solo developer without a data pipeline.
 |
|
Windowed Classifiers
 |
SVM, Random Forest, or small MLP on handcrafted statistical features.
 |
Poor
 |
Fast execution, but requires substantial data collection and feature engineering. Represents a "black box" that is opaque to manual debugging and configuration file tuning.
 |
|
Template Matchers
 |
Geometric alignment of the input stroke against 1–5 recorded exemplars ($ family, DTW).
 |
Ideal
 |
Requires only a handful of examples per class. Inherently rotation and scale invariant. Highly transparent; tuning involves simply re-recording a gesture.
 |

### The History of Shipping Systems

Shipping full-body gesture UIs, from the Kinect-era Xbox dashboard to current virtual reality system menus, rapidly abandoned complex probabilistic models in favor of lightweight geometric template matching operating over carefully segmented strokes39. The original Kinect Xbox dashboard relied heavily on explicit spatial dwells (push to click, hover to select) which induced massive fatigue. Modern systems have converged on discrete stroke gestures matched against low-fidelity templates to minimize computational overhead and maximize user predictability.

### The 2026 Recommended Architecture: Segmented Template Matching

If architecting this from scratch in 2026 under the exact constraints provided, the indisputable optimal solution is a Hybrid Segmenter-Template Pipeline utilizing an Energy-Bounded Segmenter feeding into the $P+ Recognizer.

#### The $P+ Recognizer

Created by Radu-Daniel Vatavu, the $P+ (Point-Cloud) recognizer is a milestone algorithm in the HCI gesture literature41. It treats a gesture not as a strict temporal sequence, but as a normalized 2D point cloud.

- Data Requirements: It achieves extreme accuracy (>97%) with as few as 3 to 5 training templates per command39.
- Robustness: It normalizes for scale, position, and (optionally) aspect ratio. This means a 0.2-meter exhausted wave and a 0.6-meter enthusiastic swipe are geometrically treated as identical point clouds42. Furthermore, because it matches points purely on spatial proximity rather than temporal sequence, it is highly immune to the speed variations and APA counter-motions that break velocity-based FSMs.
- Developer Experience: Tuning the system does not involve guessing metric thresholds. The developer simply records 3 new baseline waves in the living room, saves the point coordinates to the config file, and replaces the old templates.

#### The Segmenter (Machete or Kinetic Bounding)

Template matchers like $P+ require a cleanly bounded stroke. To find this stroke in the continuous 30Hz stream, a segmenter is required.

- Machete: Developed by Taranta et al., Machete is a continuous dynamic programming algorithm specifically designed to isolate custom gestures in continuous high-activity streams using only a single training sample44. It evaluates the incoming data against templates in real-time, effectively solving segmentation and recognition simultaneously without unbounded memory growth47.
- Kinetic Bounding: Alternatively, if implementing Machete is too complex, an Energy-Bounded Segmenter is highly effective. It calculates the Savitzky-Golay velocity magnitude. It triggers the start of a segment when velocity breaks the noise-floor threshold, and caps the segment at the next velocity zero-crossing (the post-stroke hold). The extracted segment is then passed to $P+.

## Designing a Non-Confusable 6-Command Vocabulary

The current vocabulary suffers from feature-space collisions. For example, a dominant-hand cross-body "Confirm" swipe is kinematically identical to a "Navigate Left" swipe. Lowering the arms to rest triggers "Navigate Down."

To ensure mutual separability in the sensor's feature space, the vocabulary must adhere to principled design rules:

- Spatial Orthogonality: Navigation commands must traverse distinct geometric paths (e.g., strictly vertical versus strictly horizontal).
- Anatomical Independence: Critical commands (like Confirm/Back) should not share the same appendage as navigation commands.
- Stroke Primacy: Retractions must never traverse the same path as a valid command.

### The Recommended 6-Command Set

We restrict all reasoning to the frontal plane ( X  = left/right,  Y  = up/down), normalized by torso length to ensure stature independence. In this schema, the Dominant Hand (DH) navigates, and the Non-Dominant Hand (NDH) controls clutching and confirmation.

|
Command
 |
Action / Kinematic Definition
 |
Non-Collision Argument
 |
|
Engage (Clutch)
 |
NDH wrist placed and held on the hip (spine-base joint).
 |
A dance routine rarely holds a wrist statically on the hip for >300ms. Anchoring to a joint removes camera-distance scaling issues.
 |
|
Nav Up
 |
DH forearm pivots upward in a vertical plane.
 |
Orthogonal to Left/Right.
 |
|
Nav Down
 |
DH forearm pivots downward in a vertical plane.
 |
Orthogonal to Left/Right. When the NDH clutch is released, the passive arm drop is completely ignored by the logic gate.
 |
|
Nav Left
 |
DH forearm pivots inward horizontally (across the body).
 |
Orthogonal to Up/Down.
 |
|
Nav Right
 |
DH forearm pivots outward horizontally (away from the body).
 |
Orthogonal to Up/Down.
 |
|
Confirm / Select
 |
NDH (while clutched on hip) performs a rapid upward/forward flick, then returns to the hip.
 |
By assigning Confirm to the non-dominant hand, it completely vacates the feature space of the navigation hand. No collision with Nav Left is possible.
 |
|
Back / Cancel
 |
DH performs a Contralateral Slash (diagonally down-and-across toward the opposite hip).
 |
This specific gesture (validated in prior Kinect research) utilizes both X and Y axes simultaneously. A diagonal slash cannot be geometrically confused with the cardinal Nav directions by the $P+ point-cloud matcher.
 |

## Closing the Loop: Feedback and Perceptual Tolerance

Because the user is operating in open-loop conditions (no physical buttons, no on-screen cursor), the user interface must provide immediate sensory feedback to compensate for the lack of haptic response. Without feedback, users naturally doubt the system, leading them to exaggerate their movements to force a response. This over-exertion leads to severe fatigue and severe kinematic drift over a 40-minute session25.

### Perceptual Latency Tolerance

For discrete mid-air menu commands, the human perceptual tolerance for latency is surprisingly forgiving compared to continuous cursor tracking. HCI research indicates that temporal confirmation delays of up to 150 to 250 milliseconds are tolerated for discrete mid-air gestures before the user consciously perceives the system as sluggish or causality is broken49.

This 200-millisecond budget is the saving grace of the architecture. It permits the use of the 5-frame Savitzky-Golay window (166 milliseconds) and allows for the computational overhead of continuous template matching without violating the user's expectation of immediacy.

### The Minimum Feedback Spec

Good feedback drastically relaxes the accuracy requirement on the recognizer itself. If the user intuitively understands what the system sees, they will subconsciously standardize their own kinematics to match the system's expectations.

- Visual Clutch Indicator: When the NDH rests on the hip, a reticle on the UI must fill radially over the 300ms quiescence period. This visually confirms the system is armed, prevents the Midas Touch, and explicitly trains the user on the necessary timing49.
- Auditory "Click": The instant the segmenter isolates a stroke and $P+ classifies it, the game must emit a crisp, low-latency UI "tick" sound. Neurologically, audio feedback is processed by the brain approximately 30 milliseconds faster than visual feedback. An immediate sound actively arrests the user's stroke, preventing them from over-extending the gesture out of doubt49.
- Kinematic Anchoring Hint: If the recognizer detects highly chaotic minimum-jerk violations or an excessively large bounding box (indicating whole-arm flailing), a brief visual prompt should flash: "Keep elbow tucked." This manages Consumed Endurance without requiring hard software lockouts.

## Phased Migration Path

Attempting to rewrite the entire recognition stack in a single weekend will result in catastrophic integration failures. The developer should follow a rigid, three-phase migration path from the heuristic FSM to the segmented template architecture.

Phase 1: Signal Conditioning Validation

- Strip out the legacy dual-rate exponential smoothing.
- Implement the 1 Euro Filter on the raw X/Y coordinates of the wrists. Tune  `f_cmin`  to approximately  1.0  Hz to stabilize the resting jitter, and  β  to approximately  0.05  to allow fast strokes to pass without lag.
- Implement the 5-point Savitzky-Golay filter to compute velocity. Run this in parallel with the old central-difference velocity and output both to a log file. The developer will immediately observe the physically impossible  9.0  m/s spikes vanish from the S-G stream.
Phase 2: Template Integration and Offline Testing

- Using the newly filtered data, ask target users to perform the 6 designated commands 5 times each in front of the sensor. Record these trajectories to disk.
- Implement the $P+ recognizer logic (which requires less than 100 lines of code).
- Feed manually segmented strokes (clipped by hand from the recorded sessions based on visual inspection) into  $P+  to verify that the algorithm classifies them with  >95%  accuracy against the templates.
Phase 3: Continuous Segmentation and Live Deployment

- Implement the NDH hip-anchor clutch logic with the 300ms 1-Euro stabilized quiescence requirement.
- Implement the kinetic-energy bounding segmenter. When the DH exceeds a velocity threshold (computed via S-G), begin recording points. When velocity drops below the threshold, close the segment.
- Pass the packaged segment points to  $P+  and trigger the corresponding key press.
- Tune the single velocity-bounding threshold in the config file to accommodate the lowest-energy acceptable "wave".
By transitioning from discrete spatial heuristics to continuous mathematical priors, the recognizer will achieve the stability required to eliminate the whack-a-mole regression cycle, providing a seamless, fatigue-resistant navigation layer for the living room.

#### Works cited

- Visual Interpretation of Hand Gestures for Human-Computer, https://www.computer.org/csdl/journal/tp/1997/07/i0677/13rRUwhpBEU
- Gesture Unit Segmentation Using Spatial-Temporal Information and, https://cdn.aaai.org/ocs/7787/7787-36712-1-PB.pdf
- (PDF) Gesture Phase Segmentation Dataset: An Extension for, https://www.researchgate.net/publication/366931427_Gesture_Phase_Segmentation_Dataset_An_Extension_for_Development_of_Gesture_Analysis_Models
- Multi-objective adversarial gesture generation, https://web.cs.ucdavis.edu/~neff/papers/Multi-objectiveAdversarialGestureGeneration_2019.pdf
- (PDF) Beyond Fitts's Law: A Three-Phase Model Predicts Movement, https://www.researchgate.net/publication/332004786_Beyond_Fitts's_Law_A_Three-Phase_Model_Predicts_Movement_Time_to_Position_an_Object_in_an_Immersive_3D_Virtual_Environment
- ballistic limit velocity: Topics by Science.gov, https://www.science.gov/topicpages/b/ballistic+limit+velocity
- TESTING BASAL GANGLIA MOTOR FUNCTIONS THROUGH ... - PMC, https://pmc.ncbi.nlm.nih.gov/articles/PMC2906399/
- Effects of limb-specific visual occlusion on bimanual reach-to-grasp, https://www.frontiersin.org/journals/psychology/articles/10.3389/fpsyg.2026.1884831/full
- Hand and arm coordination during reach to grasp after stroke, https://etheses.bham.ac.uk/4449/1/Pelton13PhD.pdf
- A common control signal and a ballistic stage can explain the ... - PMC, https://pmc.ncbi.nlm.nih.gov/articles/PMC4922467/
- (PDF) Noise and the two-thirds power Law. - ResearchGate, https://www.researchgate.net/publication/221619869_Noise_and_the_two-thirds_power_Law
- Segmenting sign language into motor primitives with Bayesian binning, https://www.frontiersin.org/journals/computational-neuroscience/articles/10.3389/fncom.2013.00068/full
- The Relationship between Curvature and Velocity in Two ... - PMC, https://pmc.ncbi.nlm.nih.gov/articles/PMC6573701/
- [PDF] Minimum-jerk, two-thirds power law, and isochrony, https://www.semanticscholar.org/paper/Minimum-jerk%2C-two-thirds-power-law%2C-and-isochrony%3A-Viviani-Flash/ea7ec6fa77e403cdc0a9b5575a7e32fd6cc79a07
- Spatiomotor dynamics of hand movements during the drawing of, https://openaccess.city.ac.uk/id/eprint/35291/1/tyler-et-al-2025-spatiomotor-dynamics-of-hand-movements-during-the-drawing-of-memory-guided-trajectories-without-visual.pdf
- Affine differential geometry and smoothness maximization as ... - arXiv, https://arxiv.org/pdf/1409.0675
- On the Emulation of Natural Movements by Humanoid Robots, https://warwick.ac.uk/fac/cross_fac/zeeman_institute/staffv2/richardson/publications/humanoids.pdf
- Reversals of anticipatory postural adjustments during voluntary, https://pmc.ncbi.nlm.nih.gov/articles/PMC1464531/
- Adaptability of anticipatory postural adjustments associated with, https://pmc.ncbi.nlm.nih.gov/articles/PMC3377909/
- Impaired posture, movement preparation, and execution during both, https://pmc.ncbi.nlm.nih.gov/articles/PMC6734070/
- Anticipatory postural adjustments to arm movement reveal complex, https://www.researchgate.net/publication/5903934_Anticipatory_postural_adjustments_to_arm_movement_reveal_complex_control_of_paraspinal_muscles_in_the_thorax
- (PDF) Postural adjustments associated with rapid voluntary arm, https://www.academia.edu/98100932/Postural_adjustments_associated_with_rapid_voluntary_arm_movements_1_Electromyographic_data
- Effects of touchscreen gesture's type and direction on finger-touch, https://www.researchgate.net/publication/315718761_Effects_of_touchscreen_gesture's_type_and_direction_on_finger-touch_input_performance_and_subjective_ratings
- Repair Traces and Runtime Failure Analysis of Public-Space ... - arXiv, https://arxiv.org/html/2607.21601v1
- Study on the Spatial Morphology of Virtual Hand Clicking Interaction, https://www.mdpi.com/2076-3417/16/14/7252
- Arm Posture Changes and Influences on Hand Controller Interaction, https://www.mdpi.com/2076-3417/12/5/2585
- Attention-Based Multimodal Framework for Athlete-Performance, https://www.mdpi.com/2306-5354/13/7/718
- Clutch & Grasp: Activation gestures and grip styles for device-based, https://www.researchgate.net/publication/372673921_Clutch_grasp_Activation_gestures_and_grip_styles_for_device-based_interaction_in_medical_spatial_augmented_reality
- Krunch Saturator - audiodev.blog, https://audiodev.blog/krunch/
- From Optical to AI-Driven Markerless Motion Capture in Motor, https://www.mdpi.com/2306-5354/13/7/776
- One Euro Filter for Noisy Signals | PDF | Low Pass Filter - Scribd, https://www.scribd.com/document/898509293/Casiez-et-al-1-eur-filter
- Ocular following in humans: Spatial properties - PMC, https://pmc.ncbi.nlm.nih.gov/articles/PMC3438696/
- Detailed space–time variations of the seismic response of the, https://academic.oup.com/gji/article/225/1/298/5982893
- A Physics-Grounded Multi-Modal Sensor Fusion Framework for, https://www.mdpi.com/1424-8220/26/11/3387
- Gesture Spotter: A Rapid Prototyping Tool for Key Gesture Spotting, https://www.computer.org/csdl/journal/tg/2022/11/09873969/1GjwKZEQiFa
- A Unified Framework for Gesture Recognition and Spatiotemporal, https://www.researchgate.net/publication/26338134_A_Unified_Framework_for_Gesture_Recognition_and_Spatiotemporal_Gesture_Segmentation
- Dynamic Boundary Time Warpingfor Sub-sequence Matching ... - arXiv, https://arxiv.org/html/2010.14464v2
- An Extension for Development of Gesture Analysis Models, https://jisis.org/wp-content/uploads/2023/01/I4.010.pdf
- Understanding Users' Gesture Input Performance with Index-Finger, https://www.researchgate.net/publication/370157101_iFAD_Gestures_Understanding_Users'_Gesture_Input_Performance_with_Index-Finger_Augmentation_Devices
- Understanding Users' Gesture Input Performance with Index-Finger, https://mintviz.usv.ro/publications/2023.CHI.1.pdf
- The Impact of Low Vision on Touch-Gesture Articulation on Mobile, https://www.computer.org/csdl/magazine/pc/2018/01/mpc2018010027/13rRUwj7cst
- 1 Description of the Recognizers, https://sites.uclouvain.be/ingenious/wp-content/uploads/2021/05/Recognizers_pseudocode.pdf
- Streamlined and Accurate Gesture Recognition with Penny Pincher, https://www.researchgate.net/publication/284068993_Streamlined_and_Accurate_Gesture_Recognition_with_Penny_Pincher
- 5 Machete: Easy, Efficient, and Precise Continuous Custom Gesture, https://www.cs.ucf.edu/icerc/isuelab/publications/pubs/Machete-final.pdf
- Machete: Easy, Efficient, and Precise Continuous Custom Gesture, https://mykola.io/publication/machete/
- VKM | ISUE Lab Research, https://www.cs.ucf.edu/icerc/isuelab/research/vkm/
- GitHub - ISUE/Machete: ACM TOCHI '21 | Easy, Efficient, and, https://github.com/ISUE/Machete
- Voight-Kampff Machine (VKM) - GitHub, https://github.com/ISUE/VKM
- Freeman, Euan (2016) Interaction techniques with novel multimodal, https://theses.gla.ac.uk/7140/7/2016FreemanPhd.pdf
- Comparing the Fidelity of Contemporary Pointing with Controller, https://www.csit.carleton.ca/~rteather/pdfs/ismar2022.pdf
- User Experience and Mid-Air Haptics: Applications, Methods and, https://seabass-flugelhorn-hcx4.squarespace.com/s/2-Georgiou.pdf
- Design guidelines for limiting and eliminating virtual reality-induced, https://3dvar.com/Souchet2023Design.pdf