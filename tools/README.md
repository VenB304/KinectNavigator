# Offline tools

`legacy.exe` crashes on its own every 30–120 s, and standing in front of the Kinect for
every rebuild is slow. So the recogniser is developed offline: capture a short real session,
then iterate against it (and against synthetic cases) with no game and no sensor.

Everything here runs the **same** `framebuffer.cpp` / `recognizer.cpp` / `gestures.cpp` /
`config.cpp` that ship in the DLL, so what you see is what the shim does.

---

## Capture a session

1. Arm capture for the game folder:

   ```
   dist\record.bat "E:\LegacyOfflinePC"
   ```

   (drops `record.flag` next to the shim; `record.bat "<folder>" off` disarms it)

2. `dist\install.bat "<game folder>"`, launch, move in front of the sensor. The shim appends
   `skcap-<timestamp>.skcap` to the game folder and notes the name in `SkeletonKey.log`. The
   game usually crashes before you're done — fine, the capture just ends early.

3. Move the `.skcap` into `tools\captures\` (git-ignored).

`.skcap` = a 24-byte header (`Recorder::Header`, `src/SkeletonKey/recorder.h`) then raw
`NUI_SKELETON_FRAME` records. Both sides `static_assert(sizeof(NUI_SKELETON_FRAME) == 2664)`,
so a struct-layout drift fails the build instead of replaying garbage. ~2.7 KB/frame ≈
5 MB/min.

---

## Replay — `SkeletonKeyReplay.exe`

Feeds a capture through the real pipeline, echoing recogniser output to the terminal and to
`SkeletonKey.log`. Key injection is forced to dry-run — nothing leaves the tool.

```
SkeletonKeyReplay capture.skcap --step         publish, wait for the recogniser, repeat
                                               -> fast AND lossless: THE mode for tuning
SkeletonKeyReplay capture.skcap --speed 1      real time, paced from frame.liTimeStamp
SkeletonKeyReplay capture.skcap --speed 4      4x
SkeletonKeyReplay capture.skcap --speed 0      unpaced (recogniser drops frames)
SkeletonKeyReplay capture.skcap --loop 3       play it 3x back to back
SkeletonKeyReplay capture.skcap --overlay      show the real on-screen HUD against the
                                               capture (forces wall-clock pacing; holds the
                                               window on the last frame until you press Enter)
```

Use `--step` for gesture work: every frame is delivered and the recogniser's clock runs off
`frame.liTimeStamp`, so cooldowns / dwell / repeat timing stay accurate however fast the
replay actually runs.

### Tuning without rebuilding

Drop a `kinectnav.ini` next to `SkeletonKeyReplay.exe` (template:
`dist\kinectnav.example.ini`) to override thresholds. `trace = 1` adds a ~15 Hz signal line
to the log:

- `nav_model = extend` → `dpad[t=..] ex= ey= r= arm= park= w= cmd= nd= cdw= occf= fired= rep=`
- `nav_model = swipe`  → `trace[t=..] ex= ey= evx= evy= ... sw= seg= pk= rs= ...`

plus `Gesture[..]:` lines on every emit. Edit, re-run, read — no build step.

---

## Live, no game — `SkeletonKeyLab.exe`

Opens the Kinect directly and pumps frames into the same recogniser. Lets you feel the
gestures on a live sensor without the crash-prone game. Needs the **Kinect for Windows
Runtime/SDK 1.8** and the sensor powered.

```
SkeletonKeyLab               recogniser only, on-screen HUD, no key injection
SkeletonKeyLab --live        also inject keys (only while the Lab console is focused)
SkeletonKeyLab --record      write a skcap-*.skcap next to the exe
```

`kinectnav.ini` next to the exe overrides thresholds, same as replay.

---

## Regression suite — `synth/synth_skcap.py`

Generates synthetic `.skcap` files (steady torso + a scripted hand path) and replays each,
asserting the recogniser's emit tally against an expected count. No Kinect, no recording.

```
python tools/synth/synth_skcap.py --run "C:/Github/KinectNavigator/dist/SkeletonKeyReplay.exe"
```

Covers idle / the four d-pad directions / command Enter+Esc / swipe L-R-U-D / confirm-hold /
back-pose / wind-up and double-swipe edge cases. **Must stay green** after any change to
`gestures.cpp` / `config.*`. The runner writes a per-case `dist/kinectnav.ini`.
