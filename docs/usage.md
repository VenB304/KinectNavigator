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

> The figures below mirror you, the way Just Dance's own pictograms do. Do each gesture with
> whichever hand you've set as your **navigation hand** (right by default); the other hand is
> only used for Confirm / Back.

## Waking it up (the clutch)

<img src="pictos/ready.png" alt="rest pose: dominant hand at the shoulder" width="150" align="right">

KinectNavigator starts **asleep** and ignores everything. To wake it, **rest your dominant
hand near your dominant shoulder** for a moment. The diagnostic HUD (if on) shows
`ASLEEP → READY`. This is also the neutral "ready" pose to come back to between moves.

It **disarms itself** once your dominant hand spends about **1.5 seconds continuously out of
play** — that is, not in the park box and not reaching into any direction. In practice that
means it sleeps when you **lower your arm**. Any time your hand touches a direction or the
park box, that timer resets.

It is **not dance-aware.** If you leave it armed and dance with that hand up and moving, it
stays awake and can send arrow keys. Those do nothing during a routine (the game only takes
the arrows in menus), and Back/Esc is off by default — but if you want it fully quiet while
dancing, just **drop your arm for a second first** to disarm it.

You can turn the clutch off entirely (`dpad_arm = 0`, or the checkbox in **More settings…**)
so it's always live — only sensible if you're navigating menus and never dancing with it on.

---

## The air d-pad — navigation

The default model (`nav_model = extend`). Picture a small **`+`** centred on your dominant
**shoulder**, in the plane facing the sensor.

### The park box

A small dead circle around your shoulder. While your hand is inside it, nothing happens —
this is "home". Reaching your hand **out past the park box** into a direction is what triggers
a key. Each direction is a **wedge** — a cone of angles — and the **gaps between the wedges are
dead**, so you don't need to aim precisely, but a diagonal that lands between two wedges does
nothing.

### Left / Right

<img src="pictos/left.png" alt="reach out to the side" width="150" align="right">

Reach your hand **straight out to the side**, past the park box — left of your shoulder for
`←`, right for `→`. Elbow roughly at shoulder height; you don't need to fully extend, just
clear the park box into the side wedge.

### Up

<img src="pictos/up.png" alt="reach straight up" width="150" align="right">

Reach **straight up** above your shoulder for `↑`. Keep it vertical — drifting outward turns
it into a side reach, drifting down-and-out turns it into `↓`.

### Down

<img src="pictos/down.png" alt="reach down and out to the side" width="150" align="right">

Reach **down *and* out to the side** for `↓` — not straight down. **Straight down is a dead
zone**: an arm hanging at your side is deliberately ignored, so "down" needs that outward
angle to tell it apart from resting.

### One press vs. hold-to-repeat

- Reach into a wedge and come back → **one key press**.
- Reach in and **hold** → after a short delay it **auto-repeats**, and the repeats speed up
  the longer you hold. Good for scrolling a long song list.
- **Stop** by bending your elbow / bringing your hand back toward the park box, or just
  dropping the arm.

---

## Command mode — Confirm and Back

Navigation only sends the arrow keys. **Enter** and **Esc** need *command mode*: put your
**non-dominant** hand on your **non-dominant** shoulder and keep it there. The HUD's
command-gate bar fills as your hand nears the shoulder and latches when it's close enough.
While it's latched, the dominant-hand reaches become Enter / Esc instead of arrows. Take the
hand off the shoulder to go back to plain navigation.

### Confirm — Enter

<img src="pictos/confirm.png" alt="off-hand on shoulder, dominant hand reached up" width="150" align="right">

With the gate held: reach the dominant hand **up or right** and **hold** it briefly. Sends
**Enter** — pick the highlighted song, start the routine, confirm a dialog.

### Back — Esc

<img src="pictos/back.png" alt="off-hand on shoulder, dominant hand down and out" width="150" align="right">

With the gate held: reach the dominant hand **down or left** and **hold** it for about
**3 seconds** — longer than Confirm, because Esc is the consequential one (it opens the pause
menu / backs out). The two-hand pose plus the long hold make it hard to trigger by accident.

If you never want Esc reachable, untick **Enable the Back gesture** in `KinectNavigator-Setup`
(or `enable_back = 0` in `kinectnav.ini`).

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
