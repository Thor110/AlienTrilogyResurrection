#pragma once

#include <filesystem>

#include "../Bootstrap/AudioSettings.h"
#include "../Bootstrap/GameplaySettings.h"
#include "../Bootstrap/KeyBindings.h"
#include "../Formats/LevelLoader.h"
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

        // Set when the "Fully Loaded" cheat was chosen. Declared after `outcome`
        // so the existing `{ PauseMenuOutcome::X }` brace-initialisations still
        // compile.
        bool cheatFullyLoaded = false;

        // Health pinned to its maximum while on.
        bool cheatMaximumHealth = false;
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
            ALTEngine::Bootstrap::KeyBindings& keyBindings,
            // Level and player state for the pause map. nullptr draws no map,
            // which is what any caller without a loaded level wants.
            const ALTEngine::Formats::LevelGeometry* level = nullptr,
            float playerGridX = 0.0f,
            float playerGridZ = 0.0f,
            float playerYaw = 0.0f,
            // Seen cells for the map's fog of war; nullptr reveals everything,
            // which is what an Auto Mapper does.
            const std::vector<uint8_t>* minimapVisited = nullptr);
    };
}
