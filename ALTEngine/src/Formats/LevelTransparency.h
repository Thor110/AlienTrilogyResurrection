#pragma once

#include <vector>

namespace ALTEngine::Formats
{
    // Level texture transparency is palette-INDEX based (not colour
    // based, unlike OPTGFX's speaker/Multitap models), and genuinely
    // different per level ID and per texture group (0-4) - confirmed
    // against Edward's AlienTrilogyMapLoader.cs GetTransparencyValues,
    // 2026, transcribed here exactly. Returns the list of palette
    // indices that should be transparent for `levelId` (e.g. 111 for
    // "1.1.1")'s texture group `groupIndex` (0-4) - empty if that
    // group has no index-based transparency for this level.
    std::vector<int> GetLevelTransparencyIndices(int levelId, int groupIndex);
}
