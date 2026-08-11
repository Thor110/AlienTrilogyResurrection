// COLOURS FOR THE END-LEVEL STATISTICS: use NEWFONT.PAL.
//
// Edward, 2026: NEWFONT.PAL holds the colours the original uses for the
// end-of-level statistics readout. It is one of the real .PAL files on the disc
// (listed in DiscFileManifest.json alongside BONESHIP / COLONY / LEGAL /
// LOGOSGFX / PRISHOLD / PANEL), so it can be loaded directly rather than guessed
// at when the statistics screen is built.
//
// Which entries map to which element is not established yet. For reference, the
// HUD took its bar green from PANEL.PAL entry 5 (RGB 72/164/72), so the same
// approach - identify the entry, use the value - applies here.

#pragma once

#include <filesystem>

namespace ALTEngine::Screens
{
    enum class EndLevelPromptChoice
    {
        Continue,
        Quit,
    };

    struct EndLevelPromptResult
    {
        bool windowClosed = false;
        EndLevelPromptChoice choice = EndLevelPromptChoice::Continue;
    };

    // The "Continue game / Save Game / Quit Game" prompt shown after the
    // end-of-level Mission Assessment count finishes - matches the
    // reference screenshot ("end-level-after-count.png"): simple
    // 3-option text menu, no model, spine background. Choosing "Save
    // Game" opens SaveSlotScreen (Save mode) right here, then returns to
    // this same prompt rather than leaving it - matches how a save
    // checkpoint naturally behaves (you can still continue or quit
    // afterward).
    //
    // No real level-progression system exists yet, so "Continue" and
    // "Quit" are just reported back to the caller to act on - there's
    // nowhere for this to hand off to yet beyond that.
    class EndLevelPromptScreen
    {
    public:
        static EndLevelPromptResult Run(const std::filesystem::path& cdDirectory);
    };
}
