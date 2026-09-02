# Setup

Installing KinectNavigator for the **Just Dance Legacy Offline PC** mod. For how to *use* it
once it's in — gestures, the diagnostic HUD, tuning — see [`usage.md`](usage.md).

## Requirements

- An **Xbox 360 Kinect (v1)** plus the **"Kinect for Windows" adapter** (the brick with the
  USB plug and power lead), already working with Legacy.
- The **Kinect for Windows Runtime v1.8** — you almost certainly have it if the sensor works
  in the game. If not: `KinectRuntime-v1.8-Setup.exe` (~111 MB),
  <https://www.microsoft.com/download/details.aspx?id=40277>.
- Room to stand about **2.5 m** back from the sensor, centred, facing it, whole body in view.

> **Not for webcam-emulator setups.** If you drive Legacy with a webcam + *Legacy Sensor*
> (itsvexor) or anything else that already replaces `Kinect10.dll` with a fake one,
> KinectNavigator can't sit on top of it — it needs the genuine ~15 MB Microsoft runtime.

---

## Install

Every method does the **same swap**: the game's real `Kinect10.dll` becomes
`Kinect10_backend.dll`, and KinectNavigator's `Kinect10.dll` takes its place. The shim forwards
every call to the renamed runtime and reads the skeleton data on the way past. It runs on
built-in defaults — **no config file is required**.

**Back up your game folder first.** `legacy.exe` is a large, unstable modded build.
Then extract this release anywhere (not inside the game folder) and pick one:

| You want… | Use |
|---|---|
| the simple way, with a window | **`KinectNavigator-Setup`** |
| a terminal / a script | `install.bat` |
| to run nothing at all | the [by-hand](#by-hand) rename |

### KinectNavigator-Setup  (recommended)

Double-click **`KinectNavigator-Setup`** (the `.vbs`; `.bat` works too). Don't double-click the
`.ps1` — Windows opens it for editing.

1. Pick your **language** from the flag menu, top-right (starts in your Windows language if
   it's one of the twelve).
2. Confirm the **game folder** — it usually finds it; otherwise **Browse…** to the folder with
   `legacy.exe`.
3. Check the status line: it should say it sees the genuine ~15 MB runtime.
4. *(optional)* Set anything under **Settings** / **More settings…** — these save to
   `kinectnav.ini` as you change them and take effect next launch, no Install needed.
5. Click **Install**.

The window is portable: it keeps its own settings in a `config.txt` beside itself and touches
nothing else. Delete the extracted folder when you're done.

### install.bat

`install.bat` does the same swap from a terminal — double-click and it prompts for the game
folder, or pass it: `install.bat "C:\path\to\game folder"`. It adds a size check and keeps a
`Kinect10.dll.orig-backup`. `uninstall.bat` reverses it.

### By hand

In your game folder (the one with `legacy.exe`):

1. Rename **`Kinect10.dll`** → **`Kinect10_backend.dll`**.
2. Copy this release's **`Kinect10.dll`** into the folder.
3. *(optional)* Copy **`kinectnav.example.ini`** in as **`kinectnav.ini`** and edit it.

Then launch the game normally. A log is written to `KinectNavigator.log` in the game folder.

---

## Uninstall

Any one of these restores the original runtime (your `kinectnav.ini` and `KinectNavigator.log`
are left alone):

- **Setup window:** open `KinectNavigator-Setup`, point it at the game folder, **Uninstall**.
- **Terminal:** `uninstall.bat "C:\path\to\game folder"`.
- **By hand:** delete the shim `Kinect10.dll`, rename `Kinect10_backend.dll` → `Kinect10.dll`.

## Updating

Extract the new release and run its `KinectNavigator-Setup` — if KinectNavigator is already
installed the button reads **Update** and just swaps in the new `Kinect10.dll`. It reads the
version from both DLLs, so it only offers Update when the download is actually newer. By hand:
just replace the shim `Kinect10.dll`.

---

## Troubleshooting

`KinectNavigator.log` in the game folder records what the recogniser saw — start there.

| Symptom | Fix |
|---|---|
| **Windows blocked the DLL / AV quarantined it** | False positive — the DLL is unsigned and synthesises keystrokes. Restore the file and add a folder exclusion, or build from source. |
| **Gestures do nothing** | The game window must be focused (click it). Confirm the sensor works in the game *without* KinectNavigator. Turn on the [diagnostic HUD](usage.md#the-diagnostic-hud) to see whether you're being tracked. |
| Can't see the HUD | It's off by default *and* hidden by exclusive fullscreen — see [usage.md](usage.md#the-diagnostic-hud). |
| Tracking looks lost (HUD says **STEP BACK** / **STEP INTO VIEW**) | Move to ~2.5 m, centre yourself, face the sensor, clear the frame. |
| **Left / right reversed** | Tick **Reverse left / right** in `KinectNavigator-Setup` (or `mirror = 1` in `kinectnav.ini`). |
| Wrong arm drives it | Set **Navigation hand** in `KinectNavigator-Setup` (or `handedness = left` / `right`). |
| It fires while you dance | Let your arm hang for ~1.5 s and it disarms. Still too eager: raise `dpad_disarm_ms`. |
| **Game crashes** | That's `legacy.exe` itself — it does this with or without KinectNavigator. Restart it. |
