#pragma once

#include "../Formats/LevelList.h"

#include <optional>
#include <string>
#include <vector>

namespace ALTEngine::Game::Progression
{
    // The campaign level that follows `levelDigits` in LevelManifest.json order,
    // or nullopt after the last one (or for a level not in the campaign).
    // Multiplayer maps are never part of the campaign.
    std::optional<Formats::LevelListEntry> NextLevel(const std::vector<Formats::LevelListEntry>& levels,
                                                     const std::string& levelDigits);

    // Looks a level up by its digits.
    std::optional<Formats::LevelListEntry> Find(const std::vector<Formats::LevelListEntry>& levels,
                                                const std::string& levelDigits);

    // Percentages for the mission assessment screen.
    struct Assessment
    {
        float aliensPercent = 0.0f;
        float secretsPercent = 0.0f;
        float missionPercent = 0.0f;
    };

    // Aliens is kills over the level's whole roster. Secrets are not tracked
    // yet and read 0; mission is 100 because reaching an End Level trigger
    // completes it - the original's own breakdown of that figure is unknown.
    Assessment Assess(int aliensKilled, int aliensTotal);
}
