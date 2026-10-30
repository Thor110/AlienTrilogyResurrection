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

TEST_CASE("Doors: a timed door opens, holds, then closes once the player has gone")
{
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    auto doors = Doors::Build(level, 30, 0.0f, 0.0f);
    REQUIRE(!doors.empty());
    DoorState& door = doors[0];           // open time 150
    REQUIRE(door.phase == DoorState::Phase::Idle);
    REQUIRE(door.threshold == 1);
    REQUIRE(door.holdTicks == 600);

    CHECK(Doors::ApplyCommand(door, /*open*/ true, 1, false));
    CHECK(Doors::Tick(door, level, door.worldX, door.worldZ) == DoorNotice::DoorActivated);
    CHECK(TickUntil(door, level, DoorState::Phase::Hold, door.worldX, door.worldZ) >= 0);
    // The step that completes the count does not raise it: one unit short.
    CHECK_EQ(door.progress, 29);
    CHECK_EQ(level.collisionGrid[door.cells[0]].ceilingHeight, static_cast<uint8_t>(door.floorUnits + 29));

    // It holds for open time * 4 ticks wherever the player is.
    const float far = 5000.0f;
    CHECK_EQ(TickUntil(door, level, DoorState::Phase::Open, door.worldX + far, door.worldZ + far, 1000), 600);

    // Standing in the doorway keeps it open.
    for (int t = 0; t < 50; ++t) { Doors::Tick(door, level, door.worldX, door.worldZ); }
    CHECK(door.phase == DoorState::Phase::Open);

    // Walking away closes it, and closing re-locks it.
    CHECK(TickUntil(door, level, DoorState::Phase::Idle, door.worldX + far, door.worldZ + far) >= 0);
    CHECK_EQ(door.progress, 0);
    CHECK_EQ(static_cast<int>(door.unlockProgress), 0);
}

TEST_CASE("Doors: a door with no open time stays open")
{
    // L111 door 5 and L112 doors 6 and 7 have an open time of 0.
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L112LEV.MAP"));
    auto doors = Doors::Build(level, 30, 0.0f, 0.0f);
    REQUIRE(doors.size() == 8);
    DoorState& door = doors[7];
    REQUIRE(door.holdTicks == 0);
    CHECK(Doors::ApplyCommand(door, false, 1, false));
    CHECK(TickUntil(door, level, DoorState::Phase::Open, 0, 0) >= 0);
    CHECK_EQ(static_cast<int>(door.unlockProgress), 0);
    for (int t = 0; t < 2000; ++t) { Doors::Tick(door, level, 1e6f, 1e6f); }
    CHECK(door.phase == DoorState::Phase::Open);

    // Unlocking it again lets it close once the player is away.
    CHECK(Doors::ApplyCommand(door, false, 1, false));
    CHECK(TickUntil(door, level, DoorState::Phase::Idle, 1e6f, 1e6f) >= 0);
}

TEST_CASE("Doors: an opening or closing door refuses commands, an open one takes them")
{
    DoorState door;
    door.threshold = 1;
    CHECK(Doors::ApplyCommand(door, false, 1, false));
    CHECK(!Doors::ApplyCommand(door, false, 1, false));   // already fully unlocked
    door.unlockProgress = 0;
    door.phase = DoorState::Phase::Opening;
    CHECK(!Doors::ApplyCommand(door, false, 1, false));   // busy
    door.phase = DoorState::Phase::Closing;
    CHECK(!Doors::ApplyCommand(door, false, 1, false));   // busy
    door.phase = DoorState::Phase::Open;
    CHECK(Doors::ApplyCommand(door, false, 1, false));
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
