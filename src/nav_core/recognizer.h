#pragma once

// M2: background thread that consumes skeleton frames from FrameBuffer, picks the
// "navigator" body (closest + most centred tracked skeleton, sticky by tracking ID),
// and logs its joints. No gesture recognition or key output yet -- that is M3+.

namespace Recognizer
{
    void Start();   // idempotent; spawns the worker thread
    void StartRich();  // same, but fed by RichBuffer (the Kinect v2 proxy); idempotent, one thread per process
    void Stop();     // signal + join. Only reached on a graceful FreeLibrary;
                     // M1 showed the game hard-exits, so usually never runs.
}
