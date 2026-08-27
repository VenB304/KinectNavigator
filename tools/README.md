# Record / replay harness

Iterate the gesture recognizer offline against captured real skeleton sessions,
so you don't have to stand in front of the Kinect for every rebuild.

## Capture

1. Turn capture on for the game folder:

   ```
   dist\record.bat "E:\LegacyOfflinePC\LegacyPC - Game"
   ```

   (creates `record.flag` next to the shim)

2. Install the shim and launch the game as usual. It writes
   `skcap-<timestamp>.skcap` into the game folder while you move in front of the
   sensor. `SkeletonKey.log` notes the file name and final frame count.

3. Turn capture off:

   ```
   dist\record.bat "E:\LegacyOfflinePC\LegacyPC - Game" off
   ```

4. Copy the `.skcap` somewhere for replay, e.g. `tools\captures\` (git-ignored).

## Replay

`SkeletonKeyReplay.exe` feeds a capture through the **same** `FrameBuffer` +
`Recognizer` code that ships in the DLL, printing recognizer output to the
terminal (and to `SkeletonKey.log` next to the exe).

```
SkeletonKeyReplay tools\captures\skcap-20260828-000131.skcap
SkeletonKeyReplay capture.skcap --speed 0      # as fast as possible
SkeletonKeyReplay capture.skcap --speed 4      # 4x
SkeletonKeyReplay capture.skcap --loop 3       # play it 3 times
```

Rebuild the recognizer, re-run replay, read the output. No sensor, no game.

## File format

24-byte header (`Recorder::Header`) then raw `NUI_SKELETON_FRAME` records
back to back. Replay reconstructs timing from `frame.liTimeStamp` (ms).
Both sides assert `sizeof(NUI_SKELETON_FRAME) == 2664`, so a layout drift
fails loudly instead of replaying garbage.

`.skcap` files are ~2.7 KB/frame ≈ 80 KB/s ≈ 5 MB/min. They are git-ignored.
