#pragma once

#include <filesystem>
#include <string>

#include "../Bootstrap/Localization.h"
#include "PlayerInventoryState.h"

namespace ALTEngine::Screens
{
    enum class GameplayOutcome
    {
        ExitGame,     // player confirmed Exit Game from the pause menu
        WindowClosed,
    };

    struct GameplayResult
    {
        GameplayOutcome outcome = GameplayOutcome::ExitGame;
    };

    // Placeholder for actual gameplay - just a black screen for now, no
    // level/minimap rendering yet (that's the next step once the pause
    // menu itself is in place). Escape opens PauseMenuScreen; confirming
    // "Exit Game" there ends this screen.
    class GameplayScreen
    {
    public:
        static GameplayResult Run(
            const std::filesystem::path& cdDirectory,
            ALTEngine::Bootstrap::Language language,
            const std::string& missionLevelCode);
    };
}
