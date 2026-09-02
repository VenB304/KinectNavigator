# Usage

How to actually use KinectNavigator once it's installed. For getting it installed, see
[`setup.md`](setup.md).

---

## Standing

Stand about **2.5 m** back from the sensor, roughly centred, facing it, with your whole body
in frame. Kinect v1 tracks poorly closer than ~2 m; at a real living-room distance your hands
are often *inferred* rather than tracked, so the recogniser leans on your shoulders and hips
(which track solidly) and on the **angle** and **reach** of your arm rather than an exact hand
position.

---

## Waking it up (the clutch)

KinectNavigator starts **asleep** and ignores everything. To wake it, **rest your dominant
hand near your dominant shoulder** for a moment. The diagnostic HUD (if on) shows
`ASLEEP → READY`.

It **disarms itself** whenever your arm just hangs at your side or you start dancing — no
input for a second or so and it's asleep again, so it can't fire in the middle of a routine.
Wake it the same way when you next need it.

You can turn the clutch off (`dpad_arm = 0`, or the checkbox in **More settings…**) so it's
always live — only useful if you're navigating menus and never dancing.

---

## The air d-pad — navigation

The default model (`nav_model = extend`). Picture a small **`+`** centred on your dominant
**shoulder**, in the plane facing the sensor.

### The park box

A small dead circle around your shoulder. While your hand is inside it, nothing happens —
this is "home". Reaching your hand **out past the park box** into a direction is what triggers
a key.

### The four directions

| Reach your hand… | Key |
|---|---|
| **out to the side** (left or right of the shoulder) | ◀ / ▶ |
| **up** | ▲ |
| **down *and* out to the side** | ▼ |

Each direction is a **wedge** — a cone of angles around straight-left / straight-right /
straight-up. The **gaps between the wedges are dead**, and so is **straight down** — a hand
hanging at your side sits in the dead straight-down zone and is ignored. "Down" deliberately
requires *down and outward* so a resting arm never triggers it.

### One press vs. hold-to-repeat

- Reach into a wedge and come back → **one key press**.
- Reach in and **hold** → after a short delay it **auto-repeats**, and the repeats speed up
  the longer you hold. Good for scrolling a long song list.
- **Stop** by bending your elbow / bringing your hand back toward the park box, or just
  dropping the arm.

---

## Command mode — Confirm and Back

Navigation only sends the arrow keys. For **Enter** and **Esc**:

1. Put your **non-dominant** hand on your **non-dominant** shoulder and keep it there.
   (The HUD's command-gate bar fills as your hand nears the shoulder; it latches when close
   enough.)
2. Now, with your **dominant** hand:

| Reach… and **hold** | Key |
|---|---|
| **up or right** | Enter — start the song / confirm |
| **down or left** | Esc — back out |

The hold is deliberate — a brief pause up/right for Enter, a longer one down/left for Esc
(Esc is the consequential one). Take the non-dominant hand off the shoulder to go back to
plain navigation.

---

## The diagnostic HUD

A small dark panel, top-left, that shows exactly what the recogniser sees. It is **off by
default** — it's a troubleshooting aid, not needed for normal play.

It shows:

- a **status line** — `ASLEEP` / `READY` / `NAVIGATING` / `COMMAND`, or
  `STEP BACK` / `STEP INTO VIEW` when tracking is poor;
- a **d-pad glyph** with the park box, the wedges, and a marker for your hand
  (grey = parked, cyan = in a nav wedge, magenta = command mode);
- your **heading and distance** from the sensor;
- the **command-gate bar** (non-dominant hand → shoulder) and, mid-hold, a
  **CONFIRM / BACK dwell bar**;
- an **action flash** when a key is sent, and one line of raw numbers for tuning.

### Turning it on

1. Run Legacy **windowed or borderless** — a layered overlay can't draw over exclusive
   fullscreen DirectX. Set `<Screen FullScreen="0" />` in the game's `config.xml`.
   (Navigation itself works the same either way; this is only so the HUD is visible.)
2. Either tick **Show the on-screen HUD** in `KinectNavigator-Setup`, or put `overlay = 1` in
   `kinectnav.ini` (copy `kinectnav.example.ini` if you don't have one).

---

## Tuning it

The easiest way is **`KinectNavigator-Setup`** — the **Settings** panel and the
**More settings…** dialog write `kinectnav.ini` in the game folder as you change them. No
reinstall; changes take effect next game launch.

**Settings** (main window):

| Control | What it does |
|---|---|
| Navigation hand | which arm navigates (the other one is the command-mode hand) |
| Reverse left / right | flips left/right output if it feels mirrored |
| Enable the Back gesture | turns Esc on/off |
| Show the on-screen HUD | the diagnostic HUD (see above) |

**More settings…**:

| Control | What it does |
|---|---|
| Reach to navigate | how far past the park box you reach before a direction registers |
| Scroll speed when held | how fast auto-repeat goes |
| Command-mode reach | how close the non-dominant hand must get to the shoulder |
| Hold time for Enter / Esc | how long you hold before Confirm / Back fires |
| Require waking it up first | the clutch — off means always live |
| Key bindings | rebind any of the six output keys (press a key to capture it) |

### Editing kinectnav.ini by hand

`kinectnav.ini` sits next to `Kinect10.dll` in the game folder. Copy `kinectnav.example.ini`
to `kinectnav.ini` and uncomment what you want — **every setting is documented in that file**,
with its default and its units (distances are torso-lengths, times in milliseconds). The DLL
re-reads it on each launch and runs fine with no file at all.

---

## Alternate navigation model — swipe

`nav_model = swipe` selects an older, motion-based model instead of the air d-pad:

- **left/right and up/down hand swipes** for navigation,
- **raise your dominant hand overhead** for Confirm,
- **non-dominant arm down and out to the side** for Back.

It runs on the same jitter-filtered hand signal as the d-pad. The **air d-pad is the default
and the tuned one**; swipe is kept as a fallback for anyone whose tracking makes the postural
d-pad awkward. Set it in `kinectnav.ini` (`nav_model = swipe`) — there's no toggle in the
setup window.
