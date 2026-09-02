# KinectNavigator

Hands-free menu navigation for the Just Dance **Legacy Offline PC** mod, driven by Kinect v1
skeleton poses — so a dancer can browse the song list and start a routine without touching a
keyboard or phone.

**Get it:** grab the latest [release](../../releases), unzip it anywhere, run
`KinectNavigator-Setup`, pick your language, click Install, close.

**Guides:** [`docs/setup.md`](docs/setup.md) — install & troubleshooting ·
[`docs/usage.md`](docs/usage.md) — gestures, the diagnostic HUD, and tuning, in depth.

## Gestures at a glance

Wake it by resting your **dominant hand near your shoulder**. It sleeps again when your arm
just hangs or you dance, so it won't fire mid-routine.

Then picture a small `+` centred on that shoulder:

| Do this | Gets you |
|---|---|
| reach your hand **out to the side** | ◀ / ▶ |
| reach **up** | ▲ |
| reach **down-and-out** to the side | ▼ |
| hold the reach | key repeats (speeds up) |
| bend the elbow / bring the hand in | stops |

For **Enter / Esc**: put your *other* hand on your *other* shoulder, then reach **up or right
and hold** (Enter) / **down or left and hold** (Esc).

![KinectNavigator gestures](dist/gestures.png)

Full walkthrough, positioning tips, and every setting: [`docs/usage.md`](docs/usage.md).

## Install

Three ways, all doing the same swap (details in [`docs/setup.md`](docs/setup.md)):

1. **`KinectNavigator-Setup`** — a small window: pick a language, point it at the game folder,
   Install. Doubles as a live editor for every setting, ships in 12 languages, shows when an
   update is available, and is fully portable (its own `config.txt` beside the exe).
2. **`install.bat` / `uninstall.bat`** — the same swap from a terminal.
3. **By hand** — rename the game's `Kinect10.dll` → `Kinect10_backend.dll`, drop the release's
   `Kinect10.dll` in its place, launch.

The shim runs on compiled-in defaults; a `kinectnav.ini` next to it overrides thresholds with
no rebuild, and is entirely optional. Players also need the **Kinect for Windows Runtime
v1.8** (see [`docs/setup.md`](docs/setup.md)).

## Diagnostic HUD

An optional dark panel (top-left) showing what the recogniser sees — tracking state, a live
d-pad, distance, action feedback. **Off by default**; a troubleshooting aid, not needed for
normal play. It's a layered window, so exclusive-fullscreen DirectX hides it — run the game
windowed / borderless to use it. Details in [`docs/usage.md`](docs/usage.md#the-diagnostic-hud).

## Build from source

Visual Studio 2022 or later, **Desktop C++** workload (x86 tools), and the **Kinect for
Windows SDK 1.8** (headers only — the DLL does not link `Kinect10.lib`; the skeleton structs
are vendored in `nui_types.h`).

```
build.cmd
```

or `msbuild src\KinectNavigator.sln /p:Configuration=Release /p:Platform=Win32`. Output is
`build\Win32\Release\Kinect10.dll`, also copied to `dist\`.

A build must keep `dumpbin` clean: exactly the 8 `Nui*` ordinals (base 5), machine x86,
imports `KERNEL32 + USER32 + GDI32 + SHELL32` only, no self-import of `Kinect10.dll`.

## How it works

KinectNavigator is a single drop-in `Kinect10.dll` that sits **between** the game and the real
Kinect runtime. It exports the eight `Nui*` ordinals `legacy.exe` imports, forwards every call
to a renamed `Kinect10_backend.dll` (the genuine Microsoft runtime — or a webcam emulator's
fake DLL), and reads the skeleton frame on its way past. A background thread turns hand pose
into arrow / Enter / Esc keystrokes with `SendInput`, only while the game window is focused.

Only the game ever opens the sensor, so the Kinect v1 "two apps can't share one sensor"
problem never comes up.

## Antivirus

`Kinect10.dll` is unsigned and it synthesises keystrokes and hooks the game's imports — both
are textbook malware behaviours, so SmartScreen or your AV may flag it. It is a false
positive. The full source is here; build it yourself if you'd rather not trust the release
binary. See [`docs/setup.md`](docs/setup.md#troubleshooting).

## Credits

- **itsvexor** — *Legacy Sensor*, the webcam→Kinect emulator for this same mod. Confirmed the
  drop-in-`Kinect10.dll` approach; not affiliated.
- Skeleton structs vendored in `nui_types.h` are the public Kinect for Windows SDK 1.8
  layouts (Microsoft), used for ABI interop only.

## License

MIT — see [`LICENSE`](LICENSE). This is clean-room interop code; it contains no Ubisoft or
Microsoft source and ships no game assets.

---

> Built with AI assistance — the recogniser was designed and iterated with Claude (Anthropic)
> and Gemini. The code, the design decisions, and every in-game test are real.
