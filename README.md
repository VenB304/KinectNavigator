# KinectNavigator — "Skeleton Key"

Hands-free menu navigation for the Just Dance **Legacy Offline PC** mod, driven by Kinect
skeleton poses. A dancer picks a song, confirms, and backs out without touching a keyboard
or phone.

Full design: [`docs/build-plan.html`](docs/build-plan.html)
(published copy: <https://claude.ai/code/artifact/15168c7b-9208-4cd4-9af7-2a13db9c9bdc>)

## How it works

Skeleton Key ships as a single drop-in `Kinect10.dll` that sits **between** the game and
whatever real Kinect back-end is in use (the genuine Microsoft runtime for real-sensor
users, or a webcam emulator's fake DLL). It forwards the eight functions `Legacy.exe`
imports to a renamed `Kinect10_backend.dll`, taps the skeleton frame on its way to the
game, runs a gesture recognizer, and synthesises the mod's menu keys via `SendInput`.

Because only the game ever opens the sensor, the Kinect v1 "two apps can't share one
sensor" problem never arises.

## Status — Milestone 1: passthrough proxy

| Milestone | State |
|-----------|-------|
| **M1** — passthrough proxy, every call logged, zero regressions | in progress |
| M2 — skeleton visible (vtable wrap + navigator-body logging) | not started |
| M3 — one gesture → one key | not started |
| M4 — full vocabulary + config + audio | not started |
| M5 — tuning pass on real hardware | not started |

M1 builds a `Kinect10.dll` that forwards all 8 imported ordinals to the real runtime and
writes a call log to `SkeletonKey.log`. No gestures, no input synthesis, no vtable patch yet.

## Build

Requires Visual Studio 2022/2026 with the **Desktop C++** workload (x86 tools) and the
**Kinect for Windows SDK 1.8** (only its headers are used; the DLL does not link
`Kinect10.lib`).

```
build.cmd
```

or manually:

```
msbuild src\SkeletonKey.sln /p:Configuration=Release /p:Platform=Win32
```

Output: `build\Win32\Release\Kinect10.dll`, also copied to `dist\`.

## Install (into a copy of the game folder)

```
cd  path\to\LegacyPC - Game
path\to\repo\dist\install.bat .
```

`install.bat` backs up the folder marker, renames the genuine `Kinect10.dll` to
`Kinect10_backend.dll`, drops Skeleton Key's `Kinect10.dll` in its place, and copies a
default `kinectnav.json`. `uninstall.bat` reverses it.

Always test against a **copy** of the game folder first.

## Layout

```
src\SkeletonKey\   the DLL project
dist\              install/uninstall scripts + default config + built DLL
docs\              design doc + field notes (M2 cadence logs, tuning sessions)
tools\             later: skeleton record/replay harness
build\             build output (git-ignored)
```
