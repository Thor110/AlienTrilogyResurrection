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
    // The "Loading data" window now does real work: incrementally
    // preloads the full PICKMOD (pause-menu weapons) and OBJ3D (level
    // objects - crates, barrels, switches, etc, per Edward 2026) model
    // catalogs, one model per frame, so gameplay never has to pay their
    // load cost - and "Hit any key to Continue..." can't appear until
    // that preload queue is actually drained, not just once the ~2
    // second minimum timer elapses. Edward, 2026: "we don't need
    // PICKMOD.BND to load until we are loading into a level... hide
    // loading both of them behind the briefing screen's loading text."
    // Actual level geometry (.MAP) loading itself still isn't wired up
    // here yet - only the two model catalogs gameplay needs.
    class MissionBriefingScreen
    {
    public:
        static MissionBriefingResult Run(
            const std::filesystem::path& cdDirectory,
            ALTEngine::Bootstrap::Language language,
            const std::string& levelCode);
    };
}
