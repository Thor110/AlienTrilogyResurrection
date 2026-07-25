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

    // Renders the actual level (real FPS camera, real geometry - see
    // Renderer/ModelRenderer.h's LoadLevel/RenderLevelToRgba) with basic
    // keyboard movement (WASD move/strafe, arrow keys look). No
    // collision yet - collision-block data isn't parsed from the .MAP
    // format yet, so movement is currently free-fly rather than
    // wall-blocked; "adjusted later to match the original gameplay if
    // necessary" per Edward, 2026. Escape opens PauseMenuScreen;
    // confirming "Exit Game" there ends this screen.
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
            ALTEngine::Bootstrap::Language language,
            const std::string& missionLevelCode);
    };
}
