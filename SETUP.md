# Skeleton Key — setup guide

Hands-free menu navigation for **Just Dance Legacy Offline PC** using an Xbox 360 Kinect.
This guide is for players. It assumes you already have the game working.

---

## 1. What you need

- **Xbox 360 Kinect sensor (Kinect v1)** plus the **"Kinect for Windows" adapter** — the
  brick that gives it a USB plug and a power lead. The bare Xbox connector won't work on a PC.
- **Kinect for Windows Runtime v1.8** — the driver. One-time install, `KinectRuntime-v1.8-Setup.exe`
  (~111 MB) from the Microsoft Download Center:
  <https://www.microsoft.com/download/details.aspx?id=40277> (the page title says
  *Kinect for Windows Runtime v1.8*). The full *Kinect for Windows SDK v1.8* (`id=40278`) also
  works if you'd rather install that.
- **Room:** you need to stand about **2.5 metres** back from the sensor, centred, facing it,
  with your whole body in view. Kinect v1 tracks poorly closer than that.

> **Not compatible with webcam-emulator setups.** If you drive Legacy with a webcam +
> *Legacy Sensor* (itsvexor) or a similar tool that already replaces `Kinect10.dll` with a
> fake one, Skeleton Key can't sit on top of it — `install.bat` needs the genuine ~15 MB
> Microsoft runtime and will refuse to run otherwise. You need a real Kinect v1 sensor.

## 2. Install the Kinect Runtime (once)

1. Run the Runtime v1.8 installer, then plug the Kinect into a **USB 2.0** port and the wall.
2. Wait for Windows to finish installing it (the light on the sensor goes solid).
3. Optional check: install the SDK too and run *Kinect Studio* or the *Skeleton Basics*
   sample to confirm your body tracks.

## 3. Set the game to windowed

Skeleton Key draws a small on-screen HUD. That HUD **cannot show over an exclusive-fullscreen
game**, so run Legacy windowed or borderless.

Open `config.xml` in the game folder and set:

```xml
<Screen FullScreen="0" />
```

(If you leave it on `"1"`, navigation still works — you just won't see the HUD.)

## 4. Install Skeleton Key

**Heads-up on antivirus.** `Kinect10.dll` is not code-signed, and it works by synthesising
keystrokes and hooking the game's imports — the same things malware does. SmartScreen may say
"Windows protected your PC" (click *More info → Run anyway*) and some antivirus may quarantine
the file (restore it / add an exclusion for the game folder). This is a false positive; the
full source is on GitHub if you'd rather build it yourself.

1. **Back up the game folder** first. `legacy.exe` is a large, unstable modded build.
2. Copy `Kinect10.dll`, `install.bat`, `uninstall.bat`, and `kinectnav.example.ini` from this
   release into the game folder (the one with `legacy.exe` in it).
3. Double-click **`install.bat`**. It will:
   - rename the game's real `Kinect10.dll` to `Kinect10_backend.dll` (and keep a
     `Kinect10.dll.orig-backup`),
   - drop Skeleton Key's `Kinect10.dll` in its place.
   It refuses to run if it doesn't see the genuine ~15 MB runtime, so it can't double-install.

That's it. Launch the game normally.

### To uninstall

Double-click **`uninstall.bat`** in the game folder. It restores the original `Kinect10.dll`.
Your `kinectnav.ini`, `SkeletonKey.log`, and any `skcap-*.skcap` files are left alone.

## 5. Playing

Stand ~2.5 m back, centred. The HUD appears top-left.

**Waking it up.** The controller starts **ASLEEP**. Rest your dominant hand near your
shoulder for a moment — the HUD switches to **READY**. It goes back to sleep on its own if
your arm just hangs or you start dancing, so it won't fire mid-routine.

**Navigating (the air d-pad).** Think of a small `+` centred on your dominant shoulder:

| Do this | Gets you |
|---|---|
| reach your hand **out to the side** | ◀ / ▶ |
| reach **up** | ▲ |
| reach **down-and-out** to the side | ▼ |
| hold the reach | key repeats (speeds up) |
| bend your elbow / bring the hand back in | stops |

A hand hanging straight down does nothing.

**Confirm / Back.** Put your **non-dominant** hand on your **non-dominant shoulder** — the HUD
turns magenta (**COMMAND**). Now:

| Do this | Gets you |
|---|---|
| reach **up or right** and hold | Enter (start the song / confirm) |
| reach **down or left** and hold | Esc (back out) |

Take the non-dominant hand off the shoulder to go back to plain navigation.

## 6. Troubleshooting

Check `SkeletonKey.log` in the game folder — it records what the recogniser saw.

| Symptom | Fix |
|---|---|
| **Windows blocked the DLL / AV quarantined it** | False positive — see the antivirus note in section 4. Restore the file and add a folder exclusion, or build from source. |
| **No HUD on screen** | The game is in exclusive fullscreen. Set `FullScreen="0"` in `config.xml`. |
| HUD says **STEP BACK** / **STEP INTO VIEW** | Move to ~2.5 m, centre yourself, face the sensor, clear the frame. |
| HUD shows but **gestures do nothing** | The game window must be focused (click it). Make sure the Kinect Runtime installed and the sensor light is solid. |
| **Left / right reversed** | Put `mirror = 1` in `kinectnav.ini`. |
| Wrong arm drives it | Put `handedness = left` (or `right`) in `kinectnav.ini`. |
| It fires while I dance | It shouldn't once it's asleep — let your arm hang for ~1.5 s and it disarms. If it's too eager, raise `dpad_disarm_ms`. |
| **Game crashes** | That's the game itself (it does this without Skeleton Key too). Restart it. |

## 7. Tweaking

Everything is adjustable without reinstalling: copy `kinectnav.example.ini` to `kinectnav.ini`
in the game folder and uncomment what you want to change. Every setting is explained in that
file. Changes take effect next launch.
