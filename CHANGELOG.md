# Changelog

## v0.9 — 2026-09-01 — first public release

Hands-free menu navigation for **Just Dance Legacy Offline PC**: a drop-in `Kinect10.dll` that
taps the skeleton stream and turns hand pose into arrow / Enter / Esc keystrokes, so you can
browse the song list and start a routine without a keyboard or phone.

### Navigation

- **Air d-pad** (`nav_model = extend`, default) — a virtual d-pad centred on your dominant
  shoulder. Reach into a direction wedge for that arrow key; hold for accelerating
  auto-repeat; a hanging or dancing arm sits in the dead zone and is ignored.
- **Clutch** — starts asleep; park your hand at your shoulder to arm it, and it disarms
  itself when your arm just hangs, so it won't fire mid-routine.
- **Command mode** — non-dominant hand on the non-dominant shoulder → reach up/right for
  Enter, down/left for Esc (held, so it's deliberate).
- **Swipe model** (`nav_model = swipe`) ships as a fallback — hand swipes for nav,
  hand-overhead for Confirm, arm-down-and-out for Back.

### Signal / robustness

- 1€ filter + 5-point Savitzky-Golay differentiator on the dominant hand — kills the
  >16 torso/s spikes Kinect v1 produces when a joint is inferred.
- Recogniser is muted while a routine is playing (detected from the game's own file I/O).

### On-screen HUD (optional, off by default)

- Compact dark panel (top-left): current state, a live d-pad, tracking distance, action
  feedback, and one line of raw numbers. A troubleshooting aid — enable with `overlay = 1`
  in `kinectnav.ini`.
- Windowed / borderless only — exclusive-fullscreen DirectX hides any overlay. Navigation
  works the same either way. `install.bat` prints a note if `config.xml` is set to fullscreen.

### Tooling (source repo)

- `SkeletonKeyReplay` — replay a captured session offline through the real recogniser
  (`--overlay` renders the HUD against a capture, no game or sensor needed).
- `SkeletonKeyLab` — run the recogniser live off the Kinect without the game.
- `tools/synth/synth_skcap.py` — synthetic-capture regression suite.

### Install

Extract the release zip into the game folder and run `install.bat`. It renames the genuine
`Kinect10.dll` to `Kinect10_backend.dll` (keeping a `.orig-backup`) and drops the shim in.
`uninstall.bat` reverses it. Full walkthrough in [`SETUP.md`](SETUP.md).

**Requires** a real Kinect v1 sensor + the *Kinect for Windows Runtime v1.8*. Not compatible
with webcam-emulator setups. The DLL is unsigned — your antivirus may flag it (false
positive; see `SETUP.md`).
