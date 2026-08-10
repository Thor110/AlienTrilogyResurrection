#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "../Bootstrap/AudioSettings.h"
#include "../Bootstrap/GameplaySettings.h"
#include "../Bootstrap/KeyBindings.h"
#include "../Bootstrap/Localization.h"
#include "../Bootstrap/RenderSettings.h"
#include "../Bootstrap/ResolutionSettings.h"

namespace ALTEngine::Menu
{
    struct MenuResult
    {
        bool windowClosed = false;
        std::string action; // "Start Game" / "Multiplayer" / "Load Game" if one was chosen

        // TEMPORARY: set alongside action == "Level Select" to the dotted level
        // code the user picked, e.g. "1.5.4". See BuildMainMenuTree.
        std::string levelCode;
    };

    // Runs the interactive main menu (and its Options subtree) until the
    // user picks a top-level action or closes the window. Renders via the
    // shared AppWindow. 3D model areas are placeholders (labeled boxes
    // showing the symbolic model name) - see MenuNode.h.
    class MenuController
    {
    public:
        static MenuResult Run(
            const std::filesystem::path& cdDirectory,
            ALTEngine::Bootstrap::RenderSettings& renderSettings,
            ALTEngine::Bootstrap::ResolutionSettings& resolutionSettings,
            ALTEngine::Bootstrap::DifficultySettings& difficultySettings,
            ALTEngine::Bootstrap::CameraSwaySettings& cameraSwaySettings,
            ALTEngine::Bootstrap::LanguageSettings& languageSettings,
            ALTEngine::Bootstrap::KeyBindings& keyBindings,
            ALTEngine::Bootstrap::AudioSettings& audioSettings,
            ALTEngine::Bootstrap::Language& language,
            std::vector<int>& mainPath,
            bool startInOptionsOnly = false);
    };
}
