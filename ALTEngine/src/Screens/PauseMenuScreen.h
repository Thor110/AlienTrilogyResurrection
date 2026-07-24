#pragma once

#include <filesystem>

#include "../Bootstrap/Localization.h"
#include "PlayerInventoryState.h"

namespace ALTEngine::Screens
{
    enum class PauseMenuOutcome
    {
        Resumed,      // player pressed Escape again / backed all the way out
        ExitGame,     // confirmed "Exit Game" -> "Yes"
        WindowClosed,
    };

    struct PauseMenuResult
    {
        PauseMenuOutcome outcome = PauseMenuOutcome::Resumed;
    };

    // The in-game pause menu, opened with Escape. Auto Mapper / Shoulder
    // Lamp / weapons / Batteries / Mission / Options down the left,
    // content on the right depending on what's highlighted - matching
    // the reference screenshots (Edward, 2026).
    class PauseMenuScreen
    {
    public:
        static PauseMenuResult Run(
            const std::filesystem::path& cdDirectory,
            ALTEngine::Bootstrap::Language language,
            const std::string& missionLevelCode,
            const PlayerInventoryState& inventory);
    };
}
