#include "TestFramework.h"

#include "Formats/LevelList.h"
#include "Game/Progression.h"

using namespace ALTEngine;

namespace
{
    std::vector<Formats::LevelListEntry> Manifest()
    {
#ifdef ALT_TEST_DATA_ROOT
        const std::filesystem::path path = std::filesystem::path(ALT_TEST_DATA_ROOT) / "LevelManifest.json";
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) { return Formats::LoadLevelList(path); }
#endif
        throw AltTest::SkipTest{ "data/LevelManifest.json not found" };
    }
}

TEST_CASE("Progression: the campaign runs in manifest order")
{
    const auto levels = Manifest();
    auto next = Game::Progression::NextLevel(levels, "111");
    REQUIRE(next.has_value());
    CHECK(next->digits == "112");
    CHECK(next->dottedCode == "1.1.2");
}

TEST_CASE("Progression: multiplayer maps are never part of the campaign")
{
    const auto levels = Manifest();
    for (const auto& entry : levels)
    {
        auto next = Game::Progression::NextLevel(levels, entry.digits);
        if (next) { CHECK(!next->multiplayer); }
        if (entry.multiplayer) { CHECK(!next.has_value()); }
    }
}

TEST_CASE("Progression: the last campaign level has no successor")
{
    const auto levels = Manifest();
    std::string last;
    for (const auto& entry : levels) { if (!entry.multiplayer) { last = entry.digits; } }
    REQUIRE(!last.empty());
    CHECK(!Game::Progression::NextLevel(levels, last).has_value());
    CHECK(!Game::Progression::NextLevel(levels, "nope").has_value());
}

TEST_CASE("Progression: the original's campaign order and its bonus levels")
{
    namespace PG = Game::Progression;
    CHECK_EQ(PG::CampaignIndex("111"), 0);
    CHECK_EQ(PG::CampaignIndex("122"), 3);    // after 113, which is a bonus
    CHECK_EQ(PG::CampaignIndex("391"), 34);
    CHECK_EQ(PG::CampaignIndex("361"), -1);   // not in the original's campaign
    CHECK_EQ(PG::CampaignIndex("900"), -1);
    for (const char* bonus : { "113", "114", "115", "155", "242", "243", "352", "353" })
    {
        CHECK(PG::IsBonusLevel(PG::CampaignIndex(bonus)));
    }
    CHECK(!PG::IsBonusLevel(PG::CampaignIndex("112")));
}

TEST_CASE("Progression: aliens, secrets and mission as the original works them")
{
    namespace PG = Game::Progression;
    PG::LevelTally t;
    t.total[2] = 20; t.killed[2] = 5;     // huggers
    t.total[6] = 8;  t.killed[6] = 3;     // warriors
    t.total[20] = 4; t.killed[20] = 4;    // 1.1.1's objectives: types 20, 25, 23
    t.total[23] = 4; t.killed[23] = 2;
    t.secretsTotal = 3; t.secretsFound = 1;
    const auto a = PG::Assess(t, 0);
    CHECK_EQ(a.aliensPercent, 28);         // 8 of 28, integer division
    CHECK_EQ(a.secretsPercent, 33);
    CHECK_EQ(a.missionPercent, 75);        // 6 of 8
    CHECK_EQ(a.requiredPercent, 15);
    CHECK_EQ(a.grade, 1);

    // Nothing to find or do is 100; nothing killed is 0, even with nothing to kill.
    const auto empty = PG::Assess(PG::LevelTally{}, 2);
    CHECK_EQ(empty.secretsPercent, 100);
    CHECK_EQ(empty.missionPercent, 100);
    CHECK_EQ(empty.aliensPercent, 0);
}

TEST_CASE("Progression: a failed mission replays, and 85% opens a bonus level")
{
    namespace PG = Game::Progression;
    PG::LevelTally t;
    t.total[22] = 10;                       // 1.1.2's switches
    t.killed[22] = 1;                       // 10%: under 15
    auto a = PG::Assess(t, 1);
    CHECK_EQ(a.grade, 0);
    CHECK_EQ(PG::NextCampaignIndex(1, a.grade), 1);

    t.killed[22] = 5;                       // 50%: passed, the bonus skipped
    a = PG::Assess(t, 1);
    CHECK_EQ(a.grade, 1);
    CHECK_EQ(PG::NextCampaignIndex(1, a.grade), 3);   // 112 -> 122

    t.killed[22] = 9;                       // 90%: the bonus level 113 next
    a = PG::Assess(t, 1);
    CHECK_EQ(a.grade, 3);
    CHECK_EQ(PG::NextCampaignIndex(1, a.grade), 2);
    CHECK_EQ(PG::NextCampaignIndex(2, 2), 3);          // and on from it

    // The required figure rises by chapter, and equalling it fails.
    CHECK_EQ(PG::Assess(PG::LevelTally{}, 12).requiredPercent, 25);
    CHECK_EQ(PG::Assess(PG::LevelTally{}, 22).requiredPercent, 45);
    PG::LevelTally q;                       // 3.1.1's objective, type 34
    q.total[34] = 20; q.killed[34] = 9;     // exactly 45%
    CHECK_EQ(PG::Assess(q, 22).grade, 0);
}
