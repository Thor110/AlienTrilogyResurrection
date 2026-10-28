#include "TestFramework.h"

#include "Formats/LevelLoader.h"

using namespace ALTEngine::Formats;

// Expected values are read off the demo disc's own level files, which is the
// closest thing to ground truth in the repository. ALTViewer's MapViewer.cs
// agrees on every field checked here.

TEST_CASE("LevelLoader: L111 header and record counts")
{
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    CHECK_EQ(level.header.mapLength, 92);
    CHECK_EQ(level.header.mapWidth, 105);
    CHECK_EQ(level.collisionGrid.size(), size_t(92 * 105));
    CHECK_EQ(level.actions.size(), size_t(64));
    CHECK_EQ(level.logics.size(), size_t(64));
    CHECK_EQ(level.doors.size(), size_t(6));
    CHECK_EQ(level.monsters.size(), size_t(28));
    CHECK_EQ(level.pickups.size(), size_t(28));
    CHECK_EQ(level.crates.size(), size_t(44));
    CHECK_EQ(level.lifts.size(), size_t(0));
}

TEST_CASE("LevelLoader: cell trigger ids index the 64-entry table directly")
{
    // A cell's action byte is the trigger id itself, not a byte offset into
    // the table. On the demo levels the values run 1..25 - ALTViewer's
    // "only ever 0-41 across every level" - and every one of them names an
    // enabled trigger. Dividing by four (as an earlier reading did) would
    // send most cells to trigger 0..6 instead.
    for (const char* file : { "SECT11/L111LEV.MAP", "SECT11/L112LEV.MAP" })
    {
        LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile(file));
        int nonZero = 0;
        for (const CollisionNode& cell : level.collisionGrid)
        {
            if (cell.scriptAction == 0) { continue; }
            nonZero++;
            REQUIRE(cell.scriptAction < level.actions.size());
            CHECK(level.actions[cell.scriptAction].commandStart != 0xFF);
        }
        CHECK(nonZero > 0);
    }
}

TEST_CASE("LevelLoader: every used trigger carries the walk-on bit 0x01")
{
    for (const char* file : { "SECT11/L111LEV.MAP", "SECT11/L112LEV.MAP" })
    {
        LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile(file));
        for (const ActionGroup& trigger : level.actions)
        {
            if (trigger.commandStart == 0xFF) { continue; }
            CHECK((trigger.activationMask & 0x01) != 0);
        }
    }
}
