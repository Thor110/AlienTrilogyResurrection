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
