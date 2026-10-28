#include "TestFramework.h"

#include "Formats/LevelLoader.h"
#include "Game/Doors.h"

using namespace ALTEngine::Formats;
using namespace ALTEngine::Game;

namespace
{
    // Runs a door until it reaches `phase`, or gives up.
    int TickUntil(DoorState& door, LevelGeometry& level, DoorState::Phase phase, float px, float pz, int limit = 500)
    {
        for (int t = 0; t < limit; ++t)
        {
            if (door.phase == phase) { return t; }
            Doors::Tick(door, level, px, pz);
        }
        return -1;
    }
}

TEST_CASE("Doors: L111 builds one closed door per record, covering four cells")
{
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    auto doors = Doors::Build(level, Doors::TravelForLevel("111"), 0.0f, 0.0f);
    REQUIRE(doors.size() == level.doors.size());
    for (const DoorState& door : doors)
    {
        CHECK_EQ(door.cellCount, 4);
        CHECK_EQ(door.travel, 30);
        for (int i = 0; i < door.cellCount; ++i)
        {
            const auto& cell = level.collisionGrid[door.cells[static_cast<size_t>(i)]];
            CHECK_EQ(cell.ceilingHeight, static_cast<uint8_t>(door.floorUnits + door.progress));
        }
    }
}

TEST_CASE("Doors: travel is 30 units on the first two levels and 43 after")
{
    CHECK_EQ(Doors::TravelForLevel("111"), 30);
    CHECK_EQ(Doors::TravelForLevel("112"), 30);
    CHECK_EQ(Doors::TravelForLevel("113"), 43);
}

TEST_CASE("Doors: an unlocked door opens, waits for the player to leave, and closes")
{
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    auto doors = Doors::Build(level, 30, 0.0f, 0.0f);
    REQUIRE(!doors.empty());
    DoorState& door = doors[0];
    REQUIRE(door.phase == DoorState::Phase::Idle);
    REQUIRE(door.threshold == 1);

    CHECK(Doors::ApplyCommand(door, /*open*/ true, 1, false));
    CHECK(Doors::Tick(door, level, door.worldX, door.worldZ) == DoorNotice::DoorActivated);
    CHECK(TickUntil(door, level, DoorState::Phase::Open, door.worldX, door.worldZ) >= 0);
    CHECK_EQ(door.progress, 30);
    CHECK_EQ(level.collisionGrid[door.cells[0]].ceilingHeight, static_cast<uint8_t>(door.floorUnits + 30));

    // Standing in the doorway holds it open.
    for (int t = 0; t < 50; ++t) { Doors::Tick(door, level, door.worldX, door.worldZ); }
    CHECK(door.phase == DoorState::Phase::Open);

    // Walking away closes it, and closing re-locks it.
    const float far = 5000.0f;
    CHECK(TickUntil(door, level, DoorState::Phase::Idle, door.worldX + far, door.worldZ + far) >= 0);
    CHECK_EQ(door.progress, 0);
    CHECK_EQ(static_cast<int>(door.unlockProgress), 0);
}

TEST_CASE("Doors: a busy or already-unlocked door refuses commands")
{
    DoorState door;
    door.threshold = 1;
    CHECK(Doors::ApplyCommand(door, false, 1, false));
    CHECK(!Doors::ApplyCommand(door, false, 1, false));   // already fully unlocked
    door.unlockProgress = 0;
    door.phase = DoorState::Phase::Opening;
    CHECK(!Doors::ApplyCommand(door, false, 1, false));   // busy
}

TEST_CASE("Doors: a two-step door cannot be opened by its own trigger first")
{
    DoorState door;
    door.threshold = 2;
    CHECK(!Doors::ApplyCommand(door, /*open*/ true, 1, false));
    CHECK(Doors::ApplyCommand(door, /*unlock*/ false, 1, /*viaSwitch*/ true));
    CHECK(door.switchOperated);
    CHECK(Doors::ApplyCommand(door, true, 1, false));
}

TEST_CASE("Doors: a switch-operated door stays open")
{
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    auto doors = Doors::Build(level, 30, 0.0f, 0.0f);
    REQUIRE(!doors.empty());
    DoorState& door = doors[0];
    CHECK(Doors::ApplyCommand(door, false, 1, true));
    CHECK(Doors::Tick(door, level, 0, 0) == DoorNotice::DoorPoweredUp);
    TickUntil(door, level, DoorState::Phase::Open, 0, 0);
    for (int t = 0; t < 200; ++t) { Doors::Tick(door, level, 1e6f, 1e6f); }
    CHECK(door.phase == DoorState::Phase::Open);
}

TEST_CASE("Doors: a door no reusable trigger can reopen stays open")
{
    // L111 door 5 is opened only by trigger 13 (one use) and L112 door 6 only
    // by trigger 8 (one use). If either closed behind the player it could never
    // open again. Doors 0 and 4 on L111 have unlimited triggers and still close.
    LevelGeometry l111 = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    auto doors = Doors::Build(l111, 30, 0.0f, 0.0f);
    Doors::MarkSingleUseDoors(l111, doors);
    REQUIRE(doors.size() == 6);
    CHECK(doors[5].staysOpen);
    CHECK(!doors[0].staysOpen);
    CHECK(!doors[4].staysOpen);

    // And it really does stay open once the player walks away.
    CHECK(Doors::ApplyCommand(doors[5], false, 1, false));
    TickUntil(doors[5], l111, DoorState::Phase::Open, 0, 0);
    for (int t = 0; t < 200; ++t) { Doors::Tick(doors[5], l111, 1e6f, 1e6f); }
    CHECK(doors[5].phase == DoorState::Phase::Open);

    LevelGeometry l112 = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L112LEV.MAP"));
    auto doors112 = Doors::Build(l112, 30, 0.0f, 0.0f);
    Doors::MarkSingleUseDoors(l112, doors112);
    REQUIRE(doors112.size() == 8);
    CHECK(doors112[6].staysOpen);
    CHECK(!doors112[0].staysOpen);
    CHECK(!doors112[7].staysOpen);
}
