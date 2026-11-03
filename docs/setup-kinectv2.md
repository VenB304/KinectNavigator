# Setup — Kinect v2

Using KinectNavigator with a **Kinect v2** (the Xbox One Kinect) instead of a v1. For the
install itself, the launchers, and uninstalling, see [`setup.md`](setup.md) — it's the same
download and the same installer for both sensors. This page is only the checklist for getting
the **v2 sensor itself** working in Windows, which is where nearly all Kinect v2 problems are.

If the sensor already works in Legacy on its own, you can skip straight to
[Install](#install).

## Requirements

- A **Kinect for Windows v2** sensor — the one-piece USB 3 sensor bar with its own power/USB
  adapter. (The Xbox One Kinect works with the Kinect Adapter for Windows.)
- **A USB 3.0 host controller that can sustain the sensor's bandwidth** (about 2.4–3.0 Gbps of
  isochronous traffic). This is the most common hardware problem — check yours before
  buying anything else:
  - **Known-good:** Intel PCH (built into most Intel chipsets), Renesas/NEC µPD720201/202,
    ASMedia ASM1142/3142.
  - **Known-bad:** ASMedia ASM1042, VIA VL80x, Fresco Logic FL110x, and AMD's integrated
    controllers are inconsistent, particularly on Windows 11. A ~$20 discrete Renesas PCIe card
    is the usual fix.
  - To see yours, in PowerShell: `Get-PnpDevice -Class USB | Where FriendlyName -match
    'Host Controller|xHCI'`. The manufacturer is in the name, or in the `VEN_####` of the
    `InstanceId` (`VEN_8086` is Intel).
  - Plug the sensor into a **rear USB 3 port on the motherboard**, directly — no front-panel
    headers, no unpowered hubs.
- **VC++ 2012 and 2013 redistributables, both x86 and x64**, installed *before* the Kinect
  runtime. Without them setup fails with *Error 1920: Kinect Monitor failed to start*.
- **Kinect for Windows Runtime 2.2** (`KinectRuntime-v2.2_1905.zip`, Microsoft Download Center,
  ID 100067). On Windows 10 1809 or newer, **don't run the MSI directly**: extract the zip,
  right-click `kinectsensor.inf` and choose **Install**.
- **Camera and microphone privacy switches ON** in Windows Settings, including "let desktop
  apps access your camera / microphone". With them off, the Kinect service keeps power-cycling
  the sensor.
- **Kinect microphone-array audio enhancements disabled**: legacy Sound control panel →
  right-click the Kinect microphone → Properties → Enhancements → disable all. Left on, they
  have crashed the Kinect service on connect, especially on Windows 11.
- **USB selective suspend off** for the sensor: Device Manager → the **WDF KinectSensor
  Interface** entries → Properties → Power Management → untick "Allow the computer to turn off
  this device to save power".
- Room to stand back from the sensor with your whole body in view. About **2.5 m** worked in
  testing, the same as v1.

> **Not for webcam-emulator setups**, same as v1: KinectNavigator needs the genuine Microsoft
> runtime. Your game folder also needs its own `Kinect20.dll` (the genuine Kinect v2 runtime);
> without one the installer skips the v2 half.

---

## Install

There is nothing separate to install for Kinect v2. The normal installer (see
[`setup.md`](setup.md)) puts in **both** proxies — `Kinect10.dll` for v1 and `Kinect20.dll` for
v2 — and renames the game's real runtimes to `Kinect10_backend.dll` / `Kinect20_backend.dll`.
Whichever sensor is plugged in when you start the game is the one KinectNavigator listens to.

The gestures are **identical on both sensors** — a v1 player and a v2 player learn the same
thing, see [`usage.md`](usage.md). Kinect v2's better tracking only makes them more reliable;
it adds no extra gestures.

To check it's working, start the game with the sensor plugged in and look in
`KinectNavigator.log` (in the game folder) for:

```
==== KinectNavigator v… Kinect v2 proxy attached ====
V2Reader: polling started
[v2 frame] #… rate=29.x Hz bodies=1 …
```

`bodies=1` appears once you stand in view. A rate near 30 Hz is normal.

---

## Troubleshooting (OS and runtime layer)

For gesture problems (nothing happens, wrong direction), see
[`setup.md`](setup.md#troubleshooting) and [`usage.md`](usage.md).

| Symptom | Likely cause |
|---|---|
| *Error 1920: Kinect Monitor failed to start* during the runtime install | The VC++ 2012/2013 redistributables are missing. Install both (x86 **and** x64) first, then retry. |
| The sensor is detected but keeps disconnecting and reconnecting | The camera/microphone privacy switches are off, or USB selective suspend is on for that controller. Check both. |
| The Kinect service crashes shortly after the sensor connects (especially Windows 11) | The microphone-array audio enhancements are on — disable them in the legacy Sound panel. |
| Choppy tracking or frequent dropouts although the sensor is detected | The USB 3 controller probably can't sustain the sensor's bandwidth. Check its chipset against the list above; a front-panel header or an unpowered hub causes this even on a good controller. |
| The runtime installer won't run on this Windows version | On Windows 10 1809 or newer, don't run the MSI: extract the zip and install through `kinectsensor.inf` (right-click → **Install**). |
| The log shows no `[v2 frame]` lines | The game didn't use the v2 path, or the proxy isn't installed. Check that your game folder has `Kinect20_backend.dll` next to a small (about 200 KB) `Kinect20.dll`, and that the sensor works without KinectNavigator. |
