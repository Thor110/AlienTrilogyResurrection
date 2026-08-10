#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    // One playable level, read from data/LevelManifest.json.
    struct LevelListEntry
    {
        std::string digits;      // "111"
        std::string dottedCode;  // "1.1.1" - the form MissionBriefingScreen and
                                 // GameplayScreen take
        std::string label;       // "1.1.1  L111LEV" - for a menu row
        std::string sectorFolder;
        std::string mapFile;
        int chapter = 0;
        int part = 0;
        bool multiplayer = false;
    };

    // Every level in manifest order: the 36 campaign levels then the 10
    // multiplayer ones. Empty if the manifest is missing or malformed.
    //
    // The manifest order matters beyond display: it is the same ordering the
    // music track table is indexed by (see Audio/MusicPlayer.h), so an index
    // here is also a level id.
    std::vector<LevelListEntry> LoadLevelList(const std::filesystem::path& manifestPath);
}
