# Research doc — threshold values (extracted from the inline images)

`docs/research/Kinect Gesture Navigation UI Design.md` renders every number as a base64 PNG
(`[image1]`..`[image19]`), so none of them are greppable. Decoded here for reference.
Source of truth is still the research doc's prose; this is just the machine-unreadable bits.

| # | Where it's used | Decoded value |
|---|---|---|
| image1  | Nav L/R activation (comparison matrix) | `ΔX > 0.15 m` |
| image2  | Nav U/D activation (comparison matrix) | `ΔY > 0.15 m` |
| image3  | Swipe velocity (matrix, Confirm/Back rows) | `Vx > 1.2 m/s` |
| image4  | Swipe distance (matrix, Confirm/Back rows) | `> 0.3 m` |
| image5  | Consumed Endurance formula | `CE = (IntTime / E(Torque)) × 100` |
| image6  | endurance-time symbol | `E` |
| image7  | hand coordinate | `(X, Y)` |
| image8  | Neutral dead zone ("area around origin") | `±10 cm` |
| image9  | activation-zone reference | `10 cm` |
| image10 | discrete-move reach example | `15 cm` |
| image11 | Swipe velocity (params table) | `Vx >` … (cut off; = `1.2 m/s`, see image3) |
| image12 | raw hand-joint jitter | `±0.1–0.3 m` |
| image13 | Engagement dwell | `600 ms` |
| image14 | Activation threshold from origin (params table) | `±15 cm` |
| image15 | Swipe distance (params table) | `ΔX >` … (cut off; = `0.3 m`, see image4) |
| image16 | Global cooldown AND auto-repeat dwell | `500 ms` |
| image17 | Auto-repeat rate | `3 Hz` |
| image18 | Auto-repeat interval | `333 ms` |
| image19 | per-frame velocity | `Vx = |X_curr − X_prev| / Δt` |

## Consolidated parameter set (research doc's recommendation)

| Parameter | Value | Notes |
|---|---|---|
| Engage: hand-above-spine + within-shoulder-width dwell | **600 ms** | drop below hip-centre = instant disengage |
| Navigation dead zone (from engage origin) | **±10 cm** X and Y |
| Navigation activation threshold | **±15 cm** X and Y | one discrete arrow key on crossing |
| Auto-repeat entry dwell (hand parked past activation) | **500 ms** |
| Auto-repeat rate | **3 Hz** (333 ms) |
| Confirm / Back swipe velocity | **> 1.2 m/s** |
| Confirm / Back swipe distance | **> 0.3 m** | cross-body, past the sternum |
| Global cooldown after a swipe | **500 ms** |
| Assumed raw hand jitter | **±0.1–0.3 m** | doc's own figure |

## Caveats before using any of these

- **Not empirically validated despite the doc's table title.** None of these trace to a
  measured source in the bibliography — they're reasonable engineering defaults. Treat as
  starting points for offline tuning, same status as the current `config.h` values.
- **Dead zone vs. jitter tension.** The doc's own raw-jitter figure (±10–30 cm) is *larger
  than the ±10 cm dead zone* and swamps the ±15 cm activation band. Only ~5 cm of hysteresis
  between "ignore" and "fire". Whether a positional model survives real v1 hand noise at
  ~2.75 m is the open question — needs live data via SkeletonKeyLab. Doc's only answer is
  "add an EMA / 1€ filter".
- **Unit mismatch with our code.** Our recognizer works in torso units (`torso = |SC−HIP|`,
  ~0.40 m measured in M2, doc assumes ~0.50 m). Rough conversions at 0.40 m:
  - ±10 cm dead zone ≈ **0.25 torso**
  - ±15 cm activation ≈ **0.375 torso**
  - 0.30 m swipe distance ≈ **0.75 torso** (~2× current `swipeDistance` 0.33 — but this is
    now a *confirm* swipe, a deliberately bigger motion than a nav swipe)
  - 1.2 m/s swipe velocity ≈ **3.0 torso/s** (current `swipeVelocity` 1.6 torso/s ≈ 0.64 m/s,
    so ~2× faster — consistent with "ballistic, isolate from casual movement")
