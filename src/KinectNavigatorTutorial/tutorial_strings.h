#pragma once
#include "framework.h"

// Compiled-in string table for KinectNavigatorTutorial's own on-screen text (step titles/
// instructions, connection screen, labels). Not the same mechanism as the GUI/Console's
// dist/app/lang/*.json -- this is a standalone exe with no JSON parser linked in, so its
// handful of player-facing strings are just baked in per language at compile time.
namespace TutStr
{
    enum class S
    {
        WelcomeTitle, WelcomeBody,
        WakeTitle, WakeBody,
        RightTitle, RightBody,
        LeftTitle, LeftBody,
        UpTitle, UpBody,
        DownTitle, DownBody,
        CmdTitle, CmdBody,
        ConfirmTitle, ConfirmBody,
        BackTitle, BackBody,
        DoneTitle, DoneBody,
        Connecting, SoftwareNotFound, NotDetected,
        SubSoftware, SubWaiting, SubMoment, EscToClose,
        LabelReference, LabelYou, LabelStepIntoView,
        Disconnected, FooterHint, WindowTitle,
        Count
    };

    enum class Lang { En, Fr, Es, De, It, Ja, Ko, Nl, Pt, Ru, ZhHans, ZhHant, Count };

    // Matches the GUI/Console's language codes (dist/app/lang/*.json base names). Unrecognized
    // or null -> En.
    Lang ParseLangCode(const wchar_t* code);

    void SetLang(Lang l);
    const wchar_t* T(S id);
}
