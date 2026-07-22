#pragma once

#include <filesystem>
#include <string>

#include "../Bootstrap/Localization.h"
#include "../Bootstrap/RenderSettings.h"

namespace ALTEngine::Menu
{
    struct MenuResult
    {
        bool windowClosed = false;
        std::string action; // "Start Game" / "Multiplayer" / "Load Game" if one was chosen
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
            ALTEngine::Bootstrap::Language& language);
    };
}
