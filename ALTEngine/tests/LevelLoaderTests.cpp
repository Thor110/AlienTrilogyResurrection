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

TEST_CASE("LevelLoader: a ceiling of 0 is put 0x30 over the floor, as the original's load does")
{
    // 0x279a4: every cell whose ceiling byte is 0 gets floor + 0x30. L111's
    // hugger duct at (14,82) is one: floor 0x40, ceiling 0 on disk.
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    const CollisionNode* duct = CellAt(level, 14, 82);
    REQUIRE(duct != nullptr);
    CHECK_EQ(duct->floorHeight, 0x40);
    CHECK_EQ(duct->ceilingHeight, 0x70);
    for (const CollisionNode& cell : level.collisionGrid)
    {
        CHECK(cell.ceilingHeight != 0 || cell.floorHeight == 0xd0);
    }
}
