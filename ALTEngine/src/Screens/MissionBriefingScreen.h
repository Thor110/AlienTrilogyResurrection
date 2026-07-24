#pragma once

#include <filesystem>
#include <string>

#include "../Bootstrap/Localization.h"

namespace ALTEngine::Screens
{
    struct MissionBriefingResult
    {
        bool windowClosed = false;
    };

    // Shows the mission briefing for `levelCode` (e.g. "1.1.1") - chapter
    // background (COLONY/PRISHOLD/BONESHIP, from the level code's first
    // digit), typed-out mission text, then a "Loading data" / "Hit any
    // key to Continue..." footer.
    //
    // There's no real level loading to wait on yet, so "loading" is
    // currently just a placeholder ~2 second timer - short enough not to
    // annoy, long enough that the state is actually visible rather than
    // flashing past unnoticed once real loading is fast. Swap this for
    // real loading progress once there's a level to load.
    class MissionBriefingScreen
    {
    public:
        static MissionBriefingResult Run(
            const std::filesystem::path& cdDirectory,
            ALTEngine::Bootstrap::Language language,
            const std::string& levelCode);
    };
}
