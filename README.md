# KinectNavigator — "Skeleton Key"

Hands-free menu navigation for the Just Dance **Legacy Offline PC** mod, driven by Kinect v1
skeleton poses — so a dancer can browse the song list and start a routine without touching a
keyboard or phone.

Design doc: [`docs/build-plan.html`](docs/build-plan.html) · Setup guide (for players):
[`SETUP.md`](SETUP.md)

## How it works

Skeleton Key is a single drop-in `Kinect10.dll` that sits **between** the game and the real
Kinect runtime. It exports the eight `Nui*` ordinals `legacy.exe` imports, forwards every call
to a renamed `Kinect10_backend.dll` (the genuine Microsoft runtime — or a webcam emulator's
fake DLL), and taps the skeleton frame on its way past. A background thread turns hand pose
into arrow / Enter / Esc keystrokes with `SendInput`, only while the game window is focused.

Only the game ever opens the sensor, so the Kinect v1 "two apps can't share one sensor"
problem never comes up.

## Navigation models

One is active at a time — pick with `nav_model` in `kinectnav.ini` (see
[`dist/kinectnav.example.ini`](dist/kinectnav.example.ini)).

### `extend` — air d-pad (default)

A virtual d-pad centred on your dominant **shoulder**. No swiping, no timing windows.

- Reach your hand out past a small **park box** into a direction **wedge** → that arrow key.
  Hold it there → auto-repeat (accelerates). Bend the elbow / return to the park box to stop.
- The gaps between wedges and straight-down are **dead**, so a hanging or dancing arm is
  ignored.
- **Clutch:** starts asleep. Park your hand at the shoulder briefly to arm it; it disarms
  itself after the arm sits idle out of play for a bit.
- **Command mode:** put your non-dominant hand on your non-dominant shoulder — now reach
  **up/right → Enter**, **down/left → Esc** (held, so it's deliberate).

### `swipe` — motion model (fallback)

Horizontal / vertical hand swipes for navigation, hand-raised-overhead for Confirm, other arm
down-and-out for Back. Runs on a 1€-filtered + Savitzky-Golay-differentiated hand signal to
kill inferred-joint jitter. Kept as a fallback; `extend` is the one that's tuned.

## On-screen HUD

A small dark overlay (top-left) shows what the recogniser sees: current state, a live d-pad,
tracking distance, and action feedback, plus one line of raw numbers for tuning. `overlay = 1`
by default.

The overlay is a layered window — **exclusive-fullscreen DirectX hides it**. Run the game
windowed or borderless (`<Screen FullScreen="0" />` in the game's `config.xml`). Navigation
itself still works in fullscreen; you just lose the HUD.

## Build

Visual Studio 2022 or later, **Desktop C++** workload (x86 tools), and the **Kinect for
Windows SDK 1.8** (headers only — the DLL does not link `Kinect10.lib`; the skeleton structs
are vendored in `nui_types.h`).

```
build.cmd
```

or `msbuild src\SkeletonKey.sln /p:Configuration=Release /p:Platform=Win32`.

Outputs to `build\Win32\Release\` (also copied to `dist\`):

| File | What |
|---|---|
| `Kinect10.dll` | the shim |
| `SkeletonKeyReplay.exe` | offline replay of a captured session (see `tools/README.md`) |
| `SkeletonKeyLab.exe` | run the recogniser live off the Kinect, without the game |

A build must keep `dumpbin` clean: exactly the 8 `Nui*` ordinals (base 5), machine x86,
imports `KERNEL32 + USER32 + GDI32 + SHELL32` only, no self-import of `Kinect10.dll`.

## Install

For players, follow [`SETUP.md`](SETUP.md). In short — into the game folder (back it up first;
`legacy.exe` is a large, unstable modded build):

```
dist\install.bat "E:\LegacyOfflinePC"
```

Renames the genuine `Kinect10.dll` → `Kinect10_backend.dll` (plus a `.orig-backup`) and drops
the shim in its place. Refuses to run unless `Kinect10.dll` is the real ~15 MB runtime.
`dist\uninstall.bat "<game folder>"` reverses it exactly.

The shim runs on compiled-in defaults; `kinectnav.ini` next to it overrides thresholds with no
rebuild.

Players also need the **Kinect for Windows Runtime v1.8** installed (see `SETUP.md`).

## Offline tuning harness

`legacy.exe` crashes on its own every 30–120 s (its bug — it faults on nearly every quit, and
a zero-keystroke capture session crashed too). So don't tune live:

1. `dist\record.bat "<game folder>"` to arm capture, play a short session — the shim writes
   `skcap-<timestamp>.skcap` into the game folder.
2. `SkeletonKeyReplay.exe <file>.skcap --step` replays it through the **same**
   `framebuffer` / `recognizer` / `gestures` code the DLL ships, echoing recogniser output.
   `--overlay` shows the real HUD against the capture (no game, no Kinect).
3. Edit `kinectnav.ini` next to the exe, re-run. `trace = 1` adds a ~15 Hz signal log.
4. `tools/synth/synth_skcap.py` generates synthetic captures and asserts recogniser output —
   the regression suite. Full details in [`tools/README.md`](tools/README.md).

## Layout

```
src/SkeletonKey/        the shim DLL
src/SkeletonKeyReplay/  offline replay tool
src/SkeletonKeyLab/     live-Kinect recogniser tool (no game)
dist/                   install / uninstall / record / package scripts, example config
docs/                   design doc, field notes (docs/notes/), research digests (docs/research/)
tools/                  replay-harness docs + the synthetic-capture regression suite
build/                  build output (git-ignored)
```

## Repo hygiene

Git-ignored: `build/`, built binaries in `dist/`, `*.skcap` / `tools/captures/`,
`record.flag`, `kinectnav.ini`, `SkeletonKey.log`, `*.zip`, Deep Research raw exports, and the
`Legacy Sensor by itsvexor*` reference folder (not redistributable).
