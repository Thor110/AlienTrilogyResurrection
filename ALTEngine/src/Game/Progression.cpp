#include "Progression.h"

#include <algorithm>

namespace ALTEngine::Game::Progression
{
    std::optional<Formats::LevelListEntry> NextLevel(const std::vector<Formats::LevelListEntry>& levels,
                                                     const std::string& levelDigits)
    {
        for (size_t i = 0; i < levels.size(); ++i)
        {
            if (levels[i].digits != levelDigits || levels[i].multiplayer) { continue; }
            for (size_t j = i + 1; j < levels.size(); ++j)
            {
                if (!levels[j].multiplayer) { return levels[j]; }
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    std::optional<Formats::LevelListEntry> Find(const std::vector<Formats::LevelListEntry>& levels,
                                                const std::string& levelDigits)
    {
        for (const auto& entry : levels)
        {
            if (entry.digits == levelDigits) { return entry; }
        }
        return std::nullopt;
    }

    const char* const CAMPAIGN_ORDER[CAMPAIGN_LENGTH] = {
        "111", "112", "113", "122", "131", "114", "141", "115", "154", "155",
        "161", "162", "211", "212", "213", "222", "242", "231", "232", "243",
        "262", "263", "311", "321", "331", "322", "351", "352", "323", "371",
        "353", "324", "381", "325", "391",
    };

    namespace
    {
        // 0xa616e: a level whose NEXT level number is listed here is followed by
        // a bonus level. The numbers are campaign indices, so these are the
        // bonus levels themselves.
        constexpr int BONUS_LEVELS[8] = { 2, 5, 7, 9, 16, 19, 27, 30 };

        // 0xa62e9: five objective types a level (-1 for none), by campaign index.
        constexpr int OBJECTIVES[CAMPAIGN_LENGTH][5] = {
            { 20, 25, 23, -1, -1 }, { 22, 24, 27, 26, -1 }, { -1, -1, -1, -1, -1 }, { 22, 24, 27, 26, -1 },
            { 35, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1 }, { 20, 25, -1, -1, -1 }, { -1, -1, -1, -1, -1 },
            { 13, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1 }, { 10, -1, -1, -1, -1 }, { 7, -1, -1, -1, -1 },
            { 21, -1, -1, -1, -1 }, { 35, -1, -1, -1, -1 }, { 34, 33, -1, -1, -1 }, { 19, 17, -1, -1, -1 },
            { -1, -1, -1, -1, -1 }, { 20, 25, -1, -1, -1 }, { 18, 16, -1, -1, -1 }, { -1, -1, -1, -1, -1 },
            { 33, -1, -1, -1, -1 }, { 7, -1, -1, -1, -1 }, { 34, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1 },
            { 5, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1 }, { 5, 35, -1, -1, -1 }, { -1, -1, -1, -1, -1 },
            { -1, -1, -1, -1, -1 }, { 13, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1 },
            { 1, -1, -1, -1, -1 }, { -1, -1, -1, -1, -1 }, { 7, -1, -1, -1, -1 },
        };

        // A share as the original works it: none done is 0, nothing to do is
        // 100, and integer division, held at 100.
        int Percent(int done, int of)
        {
            if (done == 0) { return 0; }
            if (of <= 0) { return 100; }
            return std::min(100, done * 100 / of);
        }
    }

    int CampaignIndex(const std::string& levelDigits)
    {
        for (int i = 0; i < CAMPAIGN_LENGTH; ++i)
        {
            if (levelDigits == CAMPAIGN_ORDER[i]) { return i; }
        }
        return -1;
    }

    bool IsBonusLevel(int campaignIndex)
    {
        for (int b : BONUS_LEVELS) { if (b == campaignIndex) { return true; } }
        return false;
    }

    std::vector<int> MissionObjectives(int campaignIndex)
    {
        std::vector<int> out;
        if (campaignIndex < 0 || campaignIndex >= CAMPAIGN_LENGTH) { return out; }
        for (int t : OBJECTIVES[campaignIndex]) { if (t >= 0) { out.push_back(t); } }
        return out;
    }

    Assessment Assess(const LevelTally& tally, int campaignIndex)
    {
        Assessment a;

        // Aliens: creature types 1-14.
        int killed = 0, total = 0;
        for (int type = 1; type < 15; ++type) { killed += tally.killed[type]; total += tally.total[type]; }
        a.aliensPercent = Percent(killed, total);

        // Secrets: a level with none scores 100.
        a.secretsPercent = tally.secretsTotal == 0 ? 100 : Percent(tally.secretsFound, tally.secretsTotal);

        // Mission: the level's objectives; a level with none (or none of
        // their kind) scores 100.
        int done = 0, of = 0;
        for (int type : MissionObjectives(campaignIndex))
        {
            if (type < TALLY_SIZE) { done += tally.killed[type]; of += tally.total[type]; }
        }
        a.missionPercent = of == 0 ? 100 : Percent(done, of);

        // The figure to beat: 15 in the first chapter, 25 in the second, 45 in
        // the third (by campaign index, as the original does).
        a.requiredPercent = campaignIndex > 0x15 ? 45 : (campaignIndex > 0x0b ? 25 : 15);

        // Equal to the figure is a fail; 85 or more is the best grade, and
        // opens a bonus level when one follows.
        const bool bonusNext = IsBonusLevel(campaignIndex + 1);
        if (a.missionPercent >= 85) { a.grade = bonusNext ? 3 : 2; }
        else if (a.missionPercent > a.requiredPercent) { a.grade = 1; }
        else { a.grade = 0; }
        return a;
    }

    int NextCampaignIndex(int campaignIndex, int grade)
    {
        if (grade == 0) { return campaignIndex; }
        int next = campaignIndex + 1;
        if (IsBonusLevel(next) && grade != 3) { ++next; }
        return next;
    }
}
