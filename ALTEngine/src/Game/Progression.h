#pragma once

#include "../Formats/LevelList.h"

#include <array>
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

    // ---- the original's campaign (FUN_00014f44 and the loop at 0x1150) ----
    //
    // The campaign is 35 levels in the original's own order - the level-file
    // table at 0xa4dec, indexed by the level number DAT_000b0ca8 - which is
    // not the order of their digits. Eight of them are BONUS levels (the
    // table at 0xa616e): 113, 114, 115, 155, 242, 243, 352 and 353, played
    // only after scoring 85% or more on the mission of the level before.
    inline constexpr int CAMPAIGN_LENGTH = 35;
    extern const char* const CAMPAIGN_ORDER[CAMPAIGN_LENGTH];

    // The level's place in the campaign, or -1 (multiplayer, or not one of
    // the 35).
    int CampaignIndex(const std::string& levelDigits);
    bool IsBonusLevel(int campaignIndex);

    // What a level tallies for its assessment, by type: creature types
    // 1-14 (and the vents, 0x10-0x13), then object types 0x14-0x24. A
    // creature counts toward `total` when the level builds it - those still
    // in crates included - and toward `killed` when it dies; a vent when a
    // trigger switches it off; an object when it is destroyed, or a switch
    // when it is thrown.
    inline constexpr int TALLY_SIZE = 0x25;
    struct LevelTally
    {
        std::array<int, TALLY_SIZE> killed{};
        std::array<int, TALLY_SIZE> total{};
        // Secret areas: cells carry an area number in byte 5 (CollisionNode
        // unknown6), 0 for none; the level has as many as its highest
        // number, and one is found by standing in any of its cells.
        int secretsFound = 0;
        int secretsTotal = 0;
    };

    // FUN_00014c68 and FUN_00014f44's verdict.
    struct Assessment
    {
        int aliensPercent = 0;    // types 1-14 killed of their total
        int secretsPercent = 0;   // areas found of the level's total
        int missionPercent = 0;   // the level's own objective types (0xa62e9)
        int requiredPercent = 0;  // the mission figure to beat: 15, 25 or 45 by chapter
        // 0 failed (replay the level), 1 passed, 2 passed with 85% or more,
        // 3 the same with a bonus level next, which it opens.
        int grade = 0;
    };

    Assessment Assess(const LevelTally& tally, int campaignIndex);

    // The level to play next: the same one again for a failed mission, else
    // the next, skipping a bonus level not earned. CAMPAIGN_LENGTH or more is
    // the end of the campaign.
    int NextCampaignIndex(int campaignIndex, int grade);

    // The objective types of a level (up to five), from 0xa62e9.
    std::vector<int> MissionObjectives(int campaignIndex);
}
