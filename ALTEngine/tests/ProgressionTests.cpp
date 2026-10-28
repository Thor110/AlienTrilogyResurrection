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

TEST_CASE("Progression: assessment percentages")
{
    auto a = Game::Progression::Assess(7, 28);
    CHECK(a.aliensPercent == 25.0f);
    CHECK(a.missionPercent == 100.0f);
    CHECK(Game::Progression::Assess(0, 0).aliensPercent == 100.0f);
}
