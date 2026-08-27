# Record / replay harness

Iterate the gesture recognizer offline against captured real skeleton sessions, so you
don't have to stand in front of the Kinect for every rebuild — and so a `Legacy.exe`
crash mid-session doesn't cost you the run.

## Capture

1. Turn capture on for the game folder:

   ```
   dist\record.bat "E:\LegacyOfflinePC\LegacyPC - Game"
   ```

   (creates `record.flag` next to the shim)

2. `dist\install.bat "<game folder>"`, launch the game, move in front of the sensor.
   The shim appends `skcap-<timestamp>.skcap` to the game folder; `SkeletonKey.log` notes
   the file name and, on a clean shutdown, the final frame count. The game often crashes
   before then — that's fine, replay stops cleanly at a short final record.

3. Turn capture off and clean up:

   ```
   dist\record.bat "E:\LegacyOfflinePC\LegacyPC - Game" off
   dist\uninstall.bat "E:\LegacyOfflinePC\LegacyPC - Game"
   ```

4. Move the `.skcap` into `tools\captures\` (git-ignored).

## Replay

`SkeletonKeyReplay.exe` feeds a capture through the **same** `framebuffer.cpp` +
`recognizer.cpp` + `gestures.cpp` that ship in the DLL, echoing recognizer output to the
terminal (and to `SkeletonKey.log` next to the exe). Output injection is forced to
dry-run, so no keystrokes leave the tool.

```
SkeletonKeyReplay capture.skcap --step        # publish, wait for the recognizer, repeat
                                              #   -> fast AND lossless: THE mode for tuning
SkeletonKeyReplay capture.skcap --speed 1     # real time, paced from frame.liTimeStamp
SkeletonKeyReplay capture.skcap --speed 4     # 4x
SkeletonKeyReplay capture.skcap --speed 0     # unpaced (recognizer will drop frames)
SkeletonKeyReplay capture.skcap --loop 3      # play it 3 times back to back
```

Use `--step` for gesture work — every frame is delivered, and the recognizer's clock runs
off `frame.liTimeStamp`, so timing (cooldowns, dwell, repeat intervals) is accurate no
matter how fast the replay actually runs.

### Tuning without rebuilding

Drop a `kinectnav.ini` next to `SkeletonKeyReplay.exe` (see `dist\kinectnav.example.ini`)
to override thresholds. `trace = 1` adds a ~15 Hz `trace[t=...] efx=.. evx=.. sy=.. sm=..`
line so you can see the hand signal and why a swipe did or didn't fire. Edit, re-run,
read — no build step.

## Capture on hand

`tools/captures/skcap-20260828-001358.skcap` — ~230 s, 6892 frames. Has an 83 s clean
track: scripted neutral / swipe R×4 / swipe L×4 / up×3 / down×3 / dwell×3 / back-pose×3 /
neutral / step-out-in, with ~3 s neutral pauses between blocks. The swipes in it are
**wind-up heavy**, which isn't how it'll be used in practice — good for smoke tests and for
checking the non-swipe blocks don't false-fire, not for judging swipe hit-rate.

## File format

24-byte header (`Recorder::Header` in `src/SkeletonKey/recorder.h`) then raw
`NUI_SKELETON_FRAME` records back to back. Replay reconstructs timing from
`frame.liTimeStamp` (ms). Both sides `static_assert(sizeof(NUI_SKELETON_FRAME) == 2664)`,
so a layout drift fails the build instead of replaying garbage.

`.skcap` is ~2.7 KB/frame ≈ 80 KB/s ≈ 5 MB/min. `.skcap` files and `tools/captures/` are
git-ignored.
