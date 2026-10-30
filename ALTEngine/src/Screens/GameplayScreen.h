#pragma once

#include "../Game/Progression.h"
#include <filesystem>
#include <string>

#include "../Bootstrap/AudioSettings.h"
#include "../Bootstrap/GameplaySettings.h"
#include "../Bootstrap/KeyBindings.h"
#include "../Bootstrap/Localization.h"
#include "../Bootstrap/RenderSettings.h"
#include "../Bootstrap/ResolutionSettings.h"
#include "PlayerInventoryState.h"

namespace ALTEngine::Screens
{
    enum class GameplayOutcome
    {
        ExitGame,       // player confirmed Exit Game from the pause menu
        WindowClosed,
        LevelComplete,  // an End Level trigger fired
        PlayerDied,     // the death sequence has already played
        LoadGame,       // a save was chosen from the pause menu - see loadSlot
    };

    struct GameplayResult
    {
        GameplayOutcome outcome = GameplayOutcome::ExitGame;

        // For the mission assessment, filled in on LevelComplete.
        int aliensKilled = 0;
        int aliensTotal = 0;
        // Everything the assessment counts (Game::Progression::Assess).
        ALTEngine::Game::Progression::LevelTally tally{};
        // Its game time, first tick to the End Level trigger (Modern: Level Timer).
        double levelSeconds = 0.0;

        // 1-10 when outcome is LoadGame.
        int loadSlot = 0;
    };

    // Renders the actual level (real FPS camera, real geometry - see
    // Renderer/ModelRenderer.h's LoadLevel/RenderLevelToRgba) with basic
    // movement (rebindable via keyBindings - defaults to WASD) and
    // mouse look (Edward, 2026: "mouse look rather than arrow keys for
    // the camera control"). No collision yet - collision-block data
    // isn't parsed from the .MAP format yet, so movement is currently
    // free-fly rather than wall-blocked; "adjusted later to match the
    // original gameplay if necessary" per Edward, 2026. Escape opens
    // PauseMenuScreen; confirming "Exit Game" there ends this screen.
    //
    // KNOWN OPEN QUESTION: the level header's playerStartX/playerStartY
    // fields are NOT currently used for initial camera placement -
    // their coordinate system relative to the vertex data is unresolved
    // (they're small values like 39/100, similar in scale to
    // mapWidth/mapLength, which suggests grid-cell coordinates rather
    // than the same raw unit system vertices use, which span tens of
    // thousands of units - but this isn't confirmed). The camera
    // currently starts at the level's own vertex bounding-box center
    // instead, as a safe placeholder.
    class GameplayScreen
    {
    public:
        static GameplayResult Run(
            const std::filesystem::path& cdDirectory,
            ALTEngine::Bootstrap::Language& language,
            const std::string& missionLevelCode,
            ALTEngine::Bootstrap::KeyBindings& keyBindings,
            ALTEngine::Bootstrap::AudioSettings& audioSettings,
            ALTEngine::Bootstrap::RenderSettings& renderSettings,
            ALTEngine::Bootstrap::ResolutionSettings& resolutionSettings,
            ALTEngine::Bootstrap::DifficultySettings& difficultySettings,
            ALTEngine::Bootstrap::CameraSwaySettings& cameraSwaySettings,
            ALTEngine::Bootstrap::LanguageSettings& languageSettings,
            // Owned by the caller so it can SURVIVE between levels when the
            // Modern "Keep Items" feature is on. Passing nullptr keeps the old
            // behaviour of a fresh inventory each level.
            PlayerInventoryState* carriedInventory = nullptr);
    };
}
