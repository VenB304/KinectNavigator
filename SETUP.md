# KinectNavigator — setup guide

Hands-free menu navigation for **Just Dance Legacy Offline PC** using an Xbox 360 Kinect.
This guide is for players. It assumes you already play Legacy with a working Kinect.

---

## 1. What you need

- **Xbox 360 Kinect sensor (Kinect v1)** plus the **"Kinect for Windows" adapter** (the brick
  that gives it a USB plug and a power lead), already working with Legacy.
- The **Kinect for Windows Runtime v1.8** — you almost certainly already have this if the
  sensor works in the game. If not: `KinectRuntime-v1.8-Setup.exe` (~111 MB),
  <https://www.microsoft.com/download/details.aspx?id=40277>.
- **Room:** stand about **2.5 m** back from the sensor, centred, facing it, whole body in
  view. Kinect v1 tracks poorly closer than that.

> **Not compatible with webcam-emulator setups.** If you drive Legacy with a webcam +
> *Legacy Sensor* (itsvexor) or anything else that already replaces `Kinect10.dll` with a
> fake one, KinectNavigator can't sit on top of it — the installer needs the genuine ~15 MB
> Microsoft runtime and refuses to run otherwise. You need a real Kinect v1 sensor.

## 2. Install

**Heads-up on antivirus.** `Kinect10.dll` is not code-signed, and it works by synthesising
keystrokes and hooking the game's imports — the same things malware does. SmartScreen may say
"Windows protected your PC" (click *More info → Run anyway*) and some antivirus may quarantine
the file (restore it / add a folder exclusion). This is a false positive; the full source is
on GitHub if you'd rather build it yourself. If you'd rather not run any script at all, use
the **by hand** method (2c) — it's three file operations.

First, for every method:

1. **Back up the game folder.** `legacy.exe` is a large, unstable modded build.
2. Extract this release anywhere — keep all the files together, and *not* inside the game
   folder.

All three methods do the same thing: rename the game's real `Kinect10.dll` to
`Kinect10_backend.dll` and drop KinectNavigator's `Kinect10.dll` in its place. The shim
forwards every call to that renamed runtime and taps the skeleton data on the way past. It
runs on built-in defaults — **no config file is required**.

### 2a. The setup window (recommended)

Double-click **`KinectNavigator-Setup`** — that's the `.vbs`; `.bat` also works, both open the
same window. (Don't double-click `KinectNavigator-Setup.ps1` directly — Windows opens `.ps1`
files for editing, not running.)

- Pick your **language** from the flag menu, top-right. It starts in your Windows language if
  that one is available.
- It tries to find your game folder; if it can't, click **Browse…** and pick the folder with
  `legacy.exe` in it.
- The status line confirms it sees the genuine ~15 MB Kinect runtime, notes whether the Kinect
  for Windows Runtime looks installed (best-effort — a miss doesn't block you), and — if
  KinectNavigator is already installed there — whether this download is newer.
- Optionally adjust **Settings** (navigation hand, reverse left/right, Back gesture, HUD) and
  **More settings…** (feel presets, key bindings). These write to `kinectnav.ini` in the game
  folder as you change them and take effect next launch — no need to click Install.
- Click **Install**. The **Gestures** button has the how-to-play cheat-sheet.

The window is fully portable — it keeps its own settings in a `config.txt` next to itself and
touches nothing else on your PC. Delete the extracted folder afterward and nothing is left
behind (the game-folder install is removed with **Uninstall**).

### 2b. Command line

Prefer a terminal? `install.bat` / `uninstall.bat` do the same swap — double-click and they
prompt for the game folder, or pass it as an argument:
`install.bat "C:\path\to\game folder"`. They add a size check and keep a
`Kinect10.dll.orig-backup`.

### 2c. By hand

In your game folder (the one with `legacy.exe`):

1. Rename **`Kinect10.dll`** → **`Kinect10_backend.dll`**.
2. Copy this release's **`Kinect10.dll`** into the folder.
3. *(optional)* Copy **`kinectnav.example.ini`** in as **`kinectnav.ini`** and edit it. Skip
   this and it runs on defaults.

That's it. To undo: delete the shim `Kinect10.dll`, rename `Kinect10_backend.dll` back.

---

Launch the game normally. A log is written to `KinectNavigator.log` in the game folder.

### Uninstall

- **Setup window:** open `KinectNavigator-Setup`, point it at the game folder, click
  **Uninstall**.
- **Terminal:** `uninstall.bat "C:\path\to\game folder"`.
- **By hand:** delete the shim `Kinect10.dll`, rename `Kinect10_backend.dll` → `Kinect10.dll`.

Any of these restores the original runtime. Your `kinectnav.ini` and `KinectNavigator.log` are
left in place.

### Updating to a new version

Extract the new release and run its `KinectNavigator-Setup` — if KinectNavigator is already
installed the button reads **Update** and just swaps in the new `Kinect10.dll` (no need to
uninstall first). It reads the version out of both DLLs, so it only offers Update when the
download is actually newer. By hand: just replace the shim `Kinect10.dll`.

## 3. Playing

Stand ~2.5 m back, centred, facing the sensor.

![KinectNavigator gestures](dist/gestures.png)

**Waking it up.** It starts **asleep** and ignores everything. Rest your dominant hand near
your shoulder for a moment to arm it. It disarms itself again whenever your arm just hangs or
you start dancing, so it won't fire mid-routine — re-arm the same way.

**Navigating (the air d-pad).** Picture a small `+` centred on your dominant shoulder:

| Do this | Gets you |
|---|---|
| reach your hand **out to the side** | ◀ / ▶ |
| reach **up** | ▲ |
| reach **down-and-out** to the side | ▼ |
| hold the reach | key repeats (speeds up) |
| bend your elbow / bring the hand back in | stops |

A hand hanging straight down does nothing.

**Confirm / Back.** Put your **non-dominant** hand on your **non-dominant shoulder**, then:

| Do this | Gets you |
|---|---|
| reach **up or right** and hold | Enter (start the song / confirm) |
| reach **down or left** and hold | Esc (back out) |

Take the non-dominant hand off the shoulder to go back to plain navigation.

## 4. The on-screen HUD (optional)

There's a small overlay that shows what the recogniser sees — state, a live d-pad, your
tracking distance, action feedback. It's **off by default**; it's a troubleshooting aid, not
needed for normal play.

To turn it on:

1. Run Legacy **windowed or borderless** — a layered overlay can't draw over exclusive
   fullscreen. Open `config.xml` in the game folder and set `<Screen FullScreen="0" />`.
   (Navigation works the same either way; this is only so the HUD is visible.)
2. Put `overlay = 1` in `kinectnav.ini` in the game folder (copy `kinectnav.example.ini` if
   you don't have one yet).

## 5. Troubleshooting

`KinectNavigator.log` in the game folder records what the recogniser saw — start there.

| Symptom | Fix |
|---|---|
| **Windows blocked the DLL / AV quarantined it** | False positive — see the antivirus note in section 2. Restore the file and add a folder exclusion, or build from source. |
| **Gestures do nothing** | The game window must be focused (click it). Confirm the sensor works in the game without KinectNavigator. Turn on the HUD (section 4) to see whether you're being tracked. |
| Want to see the HUD and can't | It's off by default *and* hidden by exclusive fullscreen — do both parts of section 4. |
| Tracking looks lost (HUD says **STEP BACK** / **STEP INTO VIEW**) | Move to ~2.5 m, centre yourself, face the sensor, clear the frame. |
| **Left / right reversed** | Tick **Reverse left / right** in `KinectNavigator-Setup` (or set `mirror = 1` in `kinectnav.ini`). |
| Wrong arm drives it | Set **Navigation hand** in `KinectNavigator-Setup` (or `handedness = left` / `right` in `kinectnav.ini`). |
| It fires while I dance | Let your arm hang for ~1.5 s and it disarms. If it's still too eager, raise `dpad_disarm_ms`. |
| **Game crashes** | That's the game itself (it does this without KinectNavigator too). Restart it. |

## 6. Tweaking

The easiest way is `KinectNavigator-Setup` itself — the **Settings** panel and **More settings…**
dialog write `kinectnav.ini` in the game folder as you change them, no reinstall needed.

To edit by hand instead: copy `kinectnav.example.ini` to `kinectnav.ini` in the game folder and
uncomment what you want to change. Every setting is explained in that file. Either way, changes
take effect next launch.
