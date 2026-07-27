#pragma once

#include <filesystem>

#include "../Bootstrap/AudioSettings.h"
#include "../Bootstrap/GameplaySettings.h"
#include "../Bootstrap/KeyBindings.h"
#include "../Bootstrap/Localization.h"
#include "../Bootstrap/RenderSettings.h"
#include "../Bootstrap/ResolutionSettings.h"
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
    // Lamp / weapons / Batteries / Mission / Options / Exit Game down the
    // left, content on the right depending on what's highlighted -
    // matching the reference screenshots (Edward, 2026). Options opens
    // the same full Options menu the boot menu uses (see
    // MenuController::Run's startInOptionsOnly), rather than a separate,
    // smaller copy - hence needing every setting that menu itself needs.
    class PauseMenuScreen
    {
    public:
        static PauseMenuResult Run(
            const std::filesystem::path& cdDirectory,
            ALTEngine::Bootstrap::Language& language,
            const std::string& missionLevelCode,
            const PlayerInventoryState& inventory,
            ALTEngine::Bootstrap::AudioSettings& audioSettings,
            ALTEngine::Bootstrap::RenderSettings& renderSettings,
            ALTEngine::Bootstrap::ResolutionSettings& resolutionSettings,
            ALTEngine::Bootstrap::DifficultySettings& difficultySettings,
            ALTEngine::Bootstrap::CameraSwaySettings& cameraSwaySettings,
            ALTEngine::Bootstrap::LanguageSettings& languageSettings,
            ALTEngine::Bootstrap::KeyBindings& keyBindings);
    };
}
