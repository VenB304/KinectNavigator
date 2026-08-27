# KinectNavigator — "Skeleton Key"

Hands-free menu navigation for the Just Dance **Legacy Offline PC** mod, driven by Kinect
skeleton poses — so a dancer can browse songs and pick one without a keyboard or phone.

Design doc: [`docs/build-plan.html`](docs/build-plan.html) · published copy:
<https://claude.ai/code/artifact/15168c7b-9208-4cd4-9af7-2a13db9c9bdc>

## How it works

Skeleton Key is a single drop-in `Kinect10.dll` that sits **between** the game and whatever
real Kinect back-end is present. It exports the eight `Nui*` ordinals `Legacy.exe` imports,
forwards every call to a renamed `Kinect10_backend.dll` (the genuine Microsoft runtime — or,
for webcam users, a Kinect-emulator's fake DLL), and taps the skeleton frame on its way to
the game. A background recognizer turns hand motion into menu keystrokes via `SendInput`.

Only the game ever opens the sensor, so the Kinect v1 "two apps can't share one sensor"
problem never comes up.

## Status (2026-08-28)

| Milestone | State |
|---|---|
| **M1** — passthrough proxy, every call logged, zero regressions | ✅ done, hardware-verified |
| **M2** — skeleton tap + navigator-body recognizer | ✅ done, hardware-verified |
| **Harness** — record real sessions, iterate the recognizer offline | ✅ done |
| **M3** — swipe → `←` / `→` (with hold-to-repeat) | 🔄 in iteration, not signed off |
| M4 — up/down, dwell → Confirm, pose → Back, either-hand, audio | not started |
| M5 — threshold tuning in the real play space; ship to friends | not started |
| later — Kinect v2 shim, on-screen overlay, packaging | not started |

**M3 detail:** left/right swipe fires the arrow keys and scrolls the song carousel in-game.
On its 4th build after live feedback — model went from one-swipe-per-item (too tiring) to
"swipe once = one step; swipe and leave your hand out = auto-repeat", with wave-style re-arm
(no returning to a neutral pose between gestures). Thresholds are provisional; feel not yet
judged (see the crash note). Right hand only so far — either-hand is an M4 item.

**Legacy.exe crashes on its own** — WER dumps exist from *before* this project, and it faults
on nearly every quit (silent, during teardown). One mid-session crash during M3 live testing,
undiagnosed. Not caused by the shim (a zero-keystroke session crashed too). Consequence:
don't rely on long live sessions — capture short, tune offline, confirm live briefly.
Details in the design doc.

## Build

Visual Studio 2022 / 2026, **Desktop C++** workload (x86 tools), and **Kinect for Windows
SDK 1.8** (headers only — the DLL does not link `Kinect10.lib`; the skeleton structs are
vendored in `nui_types.h`).

```
build.cmd
```

or:

```
msbuild src\SkeletonKey.sln /p:Configuration=Release /p:Platform=Win32
```

Builds two things into `build\Win32\Release\` (also copied to `dist\`):

- `Kinect10.dll` — the shim
- `SkeletonKeyReplay.exe` — the offline replay tool (see `tools/README.md`)

## Install

Into the game folder (**back it up first** — `Legacy.exe` is a large, unstable modded build):

```
dist\install.bat "E:\LegacyOfflinePC\LegacyPC - Game"
```

Renames the genuine `Kinect10.dll` → `Kinect10_backend.dll` (+ a `.orig-backup`), drops the
shim in its place. Refuses to run if `Kinect10.dll` isn't the real ~15 MB runtime.
`dist\uninstall.bat "<game folder>"` reverses it exactly.

The shim runs on compiled-in defaults. To tune, copy `dist\kinectnav.example.ini` to
`kinectnav.ini` in the game folder and edit it (no rebuild needed).

## Layout

```
src\SkeletonKey\        the shim DLL
src\SkeletonKeyReplay\  the offline replay tool
dist\                   install/uninstall scripts, record.bat, example config
docs\                   design doc + field notes
tools\                  replay-harness docs; tools\captures\ holds .skcap files (git-ignored)
build\                  build output (git-ignored)
```

## Repo hygiene

Git-ignored: `build\`, built binaries in `dist\`, `*.skcap` / `tools\captures\`, `record.flag`,
and the `Legacy Sensor by itsvexor*` reference folder + its zip (not redistributable).
