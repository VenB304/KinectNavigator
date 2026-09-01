// SkeletonKeyReplay -- feed a captured .skcap session through the real
// FrameBuffer + Recognizer, offline. Lets the recognizer be iterated without
// the game or the Kinect. Same recognizer.cpp / framebuffer.cpp that ship in
// the DLL are linked here, so what you see is what the shim would do.
//
//   SkeletonKeyReplay <file.skcap> [--step | --speed N] [--loop K]
//     --step        publish a frame, wait for the recognizer to consume it,
//                   repeat. Fast AND lossless -- the mode for gesture work.
//     --speed 1.0   real time (default), paced from frame.liTimeStamp
//     --speed 0     as fast as possible (recognizer will drop frames)
//     --speed 4     4x
//     --loop K      replay the file K times back to back

#include "framework.h"
#include "nui_types.h"
#include "framebuffer.h"
#include "recognizer.h"
#include "recorder.h"      // for Recorder::Header
#include "config.h"
#include "output.h"
#include "overlay.h"
#include "log.h"

static unsigned long long g_published = 0;

static void PlayOnce(FILE* f, double speed, bool step)
{
    NUI_SKELETON_FRAME fr;
    unsigned long long count = 0;
    LONGLONG prevTs = -1;

    while (fread(&fr, sizeof(fr), 1, f) == 1)
    {
        if (!step && speed > 0.0 && prevTs >= 0)
        {
            LONGLONG dms = fr.liTimeStamp.QuadPart - prevTs;
            if (dms < 0)    dms = 0;
            if (dms > 1000) dms = 1000;
            double s = dms / speed;
            if (s >= 1.0) Sleep((DWORD)(s + 0.5));
        }
        prevTs = fr.liTimeStamp.QuadPart;

        FrameBuffer::Publish(fr);
        ++g_published;
        ++count;

        if (step)
        {
            // wait for the recognizer thread to pick this frame up
            for (int spins = 0; FrameBuffer::ConsumedCount() < g_published; ++spins)
            {
                if (spins > 200000) { fwprintf(stderr, L"  [step] recognizer stalled\n"); break; }
                SwitchToThread();
            }
        }
    }
    wprintf(L"  (%llu frames)\n", count);
}

int wmain(int argc, wchar_t** argv)
{
    const wchar_t* path = nullptr;
    double speed = 1.0;
    int    loops = 1;
    bool   step  = false;
    bool   overlay = false;

    for (int i = 1; i < argc; ++i)
    {
        if (!wcscmp(argv[i], L"--step"))                          step  = true;
        else if (!wcscmp(argv[i], L"--overlay"))                  overlay = true;
        else if (!wcscmp(argv[i], L"--speed") && i + 1 < argc)    speed = _wtof(argv[++i]);
        else if (!wcscmp(argv[i], L"--loop")  && i + 1 < argc)    loops = _wtoi(argv[++i]);
        else if (argv[i][0] != L'-')                              path  = argv[i];
    }
    // the overlay wants wall-clock pacing -- --step would blur 6000 frames past
    // in a blink. Fall back to real-time and keep the window up at the end.
    if (overlay && step) { step = false; speed = 1.0; }
    if (!path)
    {
        fwprintf(stderr, L"usage: SkeletonKeyReplay <file.skcap> [--step | --speed N] [--loop K]\n");
        return 2;
    }

    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"rb") != 0 || !f)
    {
        fwprintf(stderr, L"cannot open %s\n", path);
        return 1;
    }

    Recorder::Header h;
    if (fread(&h, sizeof(h), 1, f) != 1 || memcmp(h.magic, "SKCAP01\n", 8) != 0)
    {
        fwprintf(stderr, L"not a SKCAP01 capture file\n");
        fclose(f);
        return 1;
    }
    if (h.frameSize != (uint32_t)sizeof(NUI_SKELETON_FRAME))
    {
        fwprintf(stderr, L"frame size mismatch: file=%u  this build=%u\n",
                 h.frameSize, (unsigned)sizeof(NUI_SKELETON_FRAME));
        fclose(f);
        return 1;
    }

    Log::SetEcho(true);
    Output::SetDryRun(true);      // never inject keys from the replay tool
    Cfg::Load();                  // kinectnav.ini next to the exe, if present
    wprintf(L"replay: %s  %s  loops=%d\n",
            path, step ? L"step" : L"timed", loops);

    Recognizer::Start();
    if (overlay) { Overlay::Start(); wprintf(L"overlay: window up (top-left)\n"); }

    const long headerEnd = ftell(f);
    for (int L = 0; L < loops; ++L)
    {
        if (L > 0) { fseek(f, headerEnd, SEEK_SET); wprintf(L"--- loop %d ---\n", L + 1); }
        PlayOnce(f, speed, step);
    }
    fclose(f);

    if (!step) Sleep(300);      // let the recognizer drain the final frame

    if (overlay)
    {
        wprintf(L"overlay held on the last frame -- press Enter to close\n");
        (void)getwchar();
        Overlay::Stop();
    }

    Recognizer::Stop();
    wprintf(L"replay complete: %llu frames published, %llu consumed\n",
            g_published, FrameBuffer::ConsumedCount());
    return 0;
}
