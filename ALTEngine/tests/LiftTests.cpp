#include "TestFramework.h"

#include "Formats/LevelLoader.h"
#include "Game/Lifts.h"

using namespace ALTEngine::Formats;
namespace L = ALTEngine::Game::Lifts;

namespace
{
    // A 4 x 4 floor with one 2 x 1 lift at (1, 1), from floor 0 to 8.
    LevelGeometry WithLift(uint8_t flags, uint8_t autoDwell, uint8_t dwell, uint8_t threshold)
    {
        LevelGeometry level;
        level.header.mapLength = 4;
        level.header.mapWidth = 4;
        level.collisionGrid.resize(16);
        Lift record;
        record.x = 1; record.y = 1; record.z = 2; record.unknown1 = 1;
        record.unknown2 = 0; record.unknown3 = 8;
        record.unknown5 = flags; record.unknown6 = autoDwell; record.unknown7 = dwell;
        record.unknown8 = threshold; record.unknown9 = 0; record.unknown10 = 3;
        level.lifts.push_back(record);
        return level;
    }
    int FloorAt(const LevelGeometry& level, int x, int z) { return level.collisionGrid[static_cast<size_t>(z * 4 + x)].floorHeight; }
}

TEST_CASE("Lifts: a lift starting at the top sets its cells there")
{
    LevelGeometry level = WithLift(L::AT_TOP, 0, 0, 1);
    auto lifts = L::Build(level);
    REQUIRE(lifts.size() == 1);
    CHECK_EQ(FloorAt(level, 1, 1), 8);
    CHECK_EQ(FloorAt(level, 2, 1), 8);
    CHECK_EQ(FloorAt(level, 3, 1), 0);   // outside it
    CHECK(lifts[0].flags & L::STARTED_HIGH);
    CHECK_EQ(lifts[0].CentreX(), 1 * 512 + 2 * 256.0f);
    CHECK_EQ(lifts[0].PlatformY(), 8 * 32 + 0x10.0p0f);
}

TEST_CASE("Lifts: the trigger counter, and the final-increment command")
{
    LevelGeometry level = WithLift(0, 0, 0, 2);
    auto lifts = L::Build(level);
    // Command 5 only counts as the step that reaches the threshold.
    CHECK(!L::AddToCounter(lifts[0], 1, true));
    CHECK(L::AddToCounter(lifts[0], 1, false));
    CHECK(L::AddToCounter(lifts[0], 1, true));
    // Due to go: further commands are refused.
    CHECK(!L::AddToCounter(lifts[0], 1, false));
}

TEST_CASE("Lifts: once triggered it rises a unit a tick, stops at the top and idles")
{
    LevelGeometry level = WithLift(0, 0, 0, 1);
    auto lifts = L::Build(level);
    auto& lift = lifts[0];
    CHECK(!L::Tick(lift, level).started);    // not triggered yet
    REQUIRE(L::AddToCounter(lift, 1, false));
    const auto first = L::Tick(lift, level);
    CHECK(first.started);
    CHECK(first.firstActivation);
    CHECK(lift.Moving());
    CHECK(!L::AddToCounter(lift, 1, false)); // moving: refused
    int ticks = 0;
    bool stopped = false;
    while (!stopped && ticks < 50) { stopped = L::Tick(lift, level).stopped; ++ticks; }
    CHECK_EQ(ticks, 8);
    CHECK_EQ(FloorAt(level, 1, 1), 8);
    CHECK(lift.flags & L::AT_TOP);
    CHECK(lift.phase == L::Phase::Idle);
    CHECK_EQ(lift.counter, 0);               // reset, ready to be called again
}

TEST_CASE("Lifts: a lift that cycles by itself waits at each end and goes back")
{
    LevelGeometry level = WithLift(0, 5, 3, 0);   // threshold 0: running from the start
    auto lifts = L::Build(level);
    auto& lift = lifts[0];
    CHECK(lift.flags & L::CYCLES);
    L::Tick(lift, level);                      // sets off up
    for (int i = 0; i < 8; ++i) { L::Tick(lift, level); }
    CHECK(lift.phase == L::Phase::Wait);
    L::Tick(lift, level); L::Tick(lift, level);
    CHECK(lift.phase == L::Phase::Wait);
    CHECK(L::Tick(lift, level).started);       // the third tick of the dwell
    CHECK(lift.phase == L::Phase::Descend);
}

TEST_CASE("Lifts: a fast lift with an odd travel still stops at its end")
{
    LevelGeometry level = WithLift(L::FAST, 0, 0, 0);
    level.lifts[0].unknown3 = 7;
    auto lifts = L::Build(level);
    int ticks = 0;
    L::Tick(lifts[0], level);
    while (!L::Tick(lifts[0], level).stopped && ticks < 20) { ++ticks; }
    CHECK_EQ(FloorAt(level, 1, 1), 7);
}
