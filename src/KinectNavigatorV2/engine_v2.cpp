// Deliberately NOT v2_framework.h: Kinect.h and nav_core's vendored nui_types.h (pulled in by
// overlay.h -> gestures.h) both define a struct named Vector4, so no TU may include both.
#include "framework.h"
#include "engine_v2.h"
#include "config.h"
#include "gameprobe.h"
#include "recognizer.h"
#include "overlay.h"
#include "recorder.h"
#include "richbuffer.h"
#include "log.h"

namespace
{
    INIT_ONCE s_once = INIT_ONCE_STATIC_INIT;

    BOOL CALLBACK InitCb(PINIT_ONCE, PVOID, PVOID*)
    {
        Cfg::Load();
        GameProbe::Install();
        Recognizer::StartRich();
        Recorder::InitRich();
        Overlay::Start();
        return TRUE;
    }
}

void EngineV2::OnFrame(const KN_RichFrame& frame)
{
    Recorder::WriteRich(frame);
    RichBuffer::Publish(frame);
}

void EngineV2::Start()
{
    InitOnceExecuteOnce(&s_once, InitCb, nullptr, nullptr);
}

void EngineV2::Stop()
{
    GameProbe::Remove();
    Overlay::Stop();
    Recognizer::Stop();
    Recorder::Close();
}
