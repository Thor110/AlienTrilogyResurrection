#include "TestFramework.h"

#include "Formats/LevelLoader.h"
#include "Game/Triggers.h"
#include "Screens/Enemies.h"

#include <string>
#include <vector>

using namespace ALTEngine::Formats;
using namespace ALTEngine::Game;
using Triggers::Source;

namespace
{
    // Records every command the trigger system hands to the world, as text, so
    // a test can compare a whole chain at once.
    struct Recorder
    {
        std::vector<std::string> log;
        bool doorsSucceed = true;
        bool objectsSucceed = true;

        Triggers::Handlers Handlers()
        {
            Triggers::Handlers h;
            h.door = [this](int door, bool open, int amount, bool viaSwitch, Source) {
                log.push_back(std::string(open ? "open " : "unlock ") + std::to_string(door) + "+" + std::to_string(amount)
                              + (viaSwitch ? " switch" : ""));
                return doorsSucceed;
            };
            h.toggleLight = [this](int light, int delta) { log.push_back("light " + std::to_string(light) + "+" + std::to_string(delta)); };
            h.changeTexture = [this](int anim, int delta) { log.push_back("texture " + std::to_string(anim) + "+" + std::to_string(delta)); };
            h.spawnMonster = [this](int monster, int amount) { log.push_back("spawn " + std::to_string(monster) + "+" + std::to_string(amount)); };
            h.activateObject = [this](int object) { log.push_back("object " + std::to_string(object)); return objectsSucceed; };
            h.endLevel = [this]() { log.push_back("end"); };
            return h;
        }
    };

    int FirstCellWithTrigger(const LevelGeometry& level, int id)
    {
        for (size_t i = 0; i < level.collisionGrid.size(); ++i)
        {
            if (level.collisionGrid[i].scriptAction == id) { return static_cast<int>(i); }
        }
        return -1;
    }

    int FirstCellWithoutTrigger(const LevelGeometry& level)
    {
        for (size_t i = 0; i < level.collisionGrid.size(); ++i)
        {
            if (level.collisionGrid[i].scriptAction == 0) { return static_cast<int>(i); }
        }
        return -1;
    }

    // A one-trigger level built by hand, for chains the demo data does not have.
    LevelGeometry SyntheticLevel(std::vector<LogicGroup> chain, uint8_t uses = Triggers::USES_UNLIMITED)
    {
        LevelGeometry level;
        level.header.mapLength = 2;
        level.header.mapWidth = 1;
        level.collisionGrid.resize(2);
        level.collisionGrid[1].scriptAction = 1;
        level.actions.assign(64, ActionGroup{ 0, 0xFF, 0xFF, 0xFF });
        level.logics.assign(64, LogicGroup{ 0xFF, 0xFF, 0xFF, 0xFF });
        level.actions[1] = ActionGroup{ Triggers::FLAG_PLAYER, 0, uses, 0 };
        for (size_t i = 0; i < chain.size(); ++i)
        {
            chain[i].nextStep = (i + 1 < chain.size()) ? static_cast<uint8_t>(i + 1) : Triggers::CHAIN_END;
            level.logics[i] = chain[i];
        }
        return level;
    }
}

TEST_CASE("Triggers: walking onto a spawn trigger spawns, once per use")
{
    // L111 trigger 6 is a single Spawn Monster (monster 3) with one use.
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    Triggers::System system(level);
    Recorder rec;
    const auto handlers = rec.Handlers();

    const int cell = FirstCellWithTrigger(level, 6);
    const int plain = FirstCellWithoutTrigger(level);
    REQUIRE(cell >= 0 && plain >= 0);

    int latch = 0;
    CHECK(system.OnEnterCell(latch, cell, handlers));
    REQUIRE(rec.log.size() == 1);
    CHECK(rec.log[0] == "spawn 3+1");
    CHECK_EQ(system.UsesRemaining(6), 0);

    // Standing on it fires nothing more; leaving and coming back finds it spent.
    CHECK(!system.OnEnterCell(latch, cell, handlers));
    system.OnEnterCell(latch, plain, handlers);
    CHECK(!system.OnEnterCell(latch, cell, handlers));
    CHECK_EQ(rec.log.size(), size_t(1));
}

TEST_CASE("Triggers: an unlimited trigger re-fires on every re-entry, not every tick")
{
    // L111 trigger 9 is the level exit, uses 0xFF.
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    Triggers::System system(level);
    Recorder rec;
    const auto handlers = rec.Handlers();
    const int cell = FirstCellWithTrigger(level, 9);
    const int plain = FirstCellWithoutTrigger(level);
    REQUIRE(cell >= 0);

    int latch = 0;
    system.OnEnterCell(latch, cell, handlers);
    system.OnEnterCell(latch, cell, handlers);
    system.OnEnterCell(latch, plain, handlers);
    system.OnEnterCell(latch, cell, handlers);
    CHECK_EQ(rec.log.size(), size_t(2));
    CHECK(rec.log[0] == "end");
    CHECK_EQ(system.UsesRemaining(9), 0xFF);
}

TEST_CASE("Triggers: a switch chain needs the use key and runs in order")
{
    // L111 trigger 3: throw switch object 8, then unlock door 3, three lights,
    // two texture animators. One use.
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    Triggers::System system(level);
    Recorder rec;
    const auto handlers = rec.Handlers();

    // Walking on cannot throw a switch, so the chain stops before anything
    // happens and the trigger is not used up.
    CHECK(!system.Run(3, Source::WalkOn, handlers));
    CHECK(rec.log.empty());
    CHECK_EQ(system.UsesRemaining(3), 1);

    CHECK(system.Run(3, Source::Interact, handlers));
    const std::vector<std::string> expected{
        "object 8", "unlock 3+1 switch", "light 112+1", "light 113+1", "light 119+1", "texture 4+1", "texture 3+1" };
    CHECK(rec.log == expected);
    CHECK_EQ(system.UsesRemaining(3), 0);
}

TEST_CASE("Triggers: a failed command aborts the rest of the chain")
{
    // L111 trigger 2: open door 2, then three lights.
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    Triggers::System system(level);
    Recorder rec;
    rec.doorsSucceed = false;
    const auto handlers = rec.Handlers();

    CHECK(!system.Run(2, Source::Interact, handlers));
    CHECK_EQ(rec.log.size(), size_t(1));   // the door attempt, nothing after it

    rec.doorsSucceed = true;
    rec.log.clear();
    CHECK(system.Run(2, Source::Interact, handlers));
    CHECK_EQ(rec.log.size(), size_t(4));
}

TEST_CASE("Triggers: a chain running into an empty command slot completes")
{
    // L111 trigger 7 is door 4 + three lights and then an all-0xFF slot.
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    Triggers::System system(level);
    Recorder rec;
    CHECK(system.Run(7, Source::Interact, rec.Handlers()));
    CHECK_EQ(rec.log.size(), size_t(4));
}

TEST_CASE("Triggers: L112's five-use trigger spawns its whole group each time")
{
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L112LEV.MAP"));
    Triggers::System system(level);
    Recorder rec;
    const auto handlers = rec.Handlers();
    for (int i = 0; i < 6; ++i) { system.Run(15, Source::WalkOn, handlers); }
    CHECK_EQ(rec.log.size(), size_t(5 * 6));   // five uses of six spawns; the sixth run does nothing
    CHECK_EQ(system.UsesRemaining(15), 0);
}

TEST_CASE("Triggers: spawn triggers release L111's waiting monsters")
{
    // End to end with the real roster: every monster that L111 holds back for
    // a spawn trigger is released once those triggers have been walked over
    // as many times as they allow.
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    auto enemies = ALTEngine::Screens::Enemies::Build(level, 0, 0.0f, 0.0f);
    Triggers::System system(level);
    Triggers::Handlers handlers;
    handlers.spawnMonster = [&](int monster, int amount) {
        ALTEngine::Screens::Enemies::TriggerSpawn(enemies, monster, amount);
    };

    for (int id : { 6, 11, 12 })
    {
        CHECK(!enemies.empty());
        for (int i = 0; i < 4; ++i) { system.Run(id, Source::WalkOn, handlers); }
    }
    for (int monster : { 3, 13, 8 })
    {
        REQUIRE(static_cast<size_t>(monster) < enemies.size());
        CHECK(enemies[static_cast<size_t>(monster)].active);
    }
}

TEST_CASE("Triggers: a lift command with nothing to drive fails the chain")
{
    LevelGeometry level = SyntheticLevel({ { Triggers::LIFT_FINAL, 0, 1, 0 }, { Triggers::TOGGLE_LIGHT, 0, 1, 5 } });
    Triggers::System system(level);
    Recorder rec;
    CHECK(!system.Run(1, Source::Interact, rec.Handlers()));
    CHECK(rec.log.empty());
}

TEST_CASE("Triggers: a looping chain is cut off instead of hanging")
{
    LevelGeometry level = SyntheticLevel({ { Triggers::TOGGLE_LIGHT, 0, 1, 5 } });
    level.logics[0].nextStep = 0;   // points at itself
    Triggers::System system(level);
    Recorder rec;
    CHECK(!system.Run(1, Source::Interact, rec.Handlers()));
    CHECK_EQ(rec.log.size(), size_t(Triggers::MAX_CHAIN_LENGTH));
}

TEST_CASE("Triggers: a trigger without the player flag never fires")
{
    LevelGeometry level = SyntheticLevel({ { Triggers::END_LEVEL, 0, 1, 0 } });
    level.actions[1].activationMask = 0x02;
    Triggers::System system(level);
    Recorder rec;
    int latch = 0;
    CHECK(!system.OnEnterCell(latch, 1, rec.Handlers()));
    CHECK(rec.log.empty());
}

TEST_CASE("Triggers: a round sets off flag-0x02 triggers, once per round")
{
    // L111 trigger 12 has flags 0x03 - walk OR shoot - and spawns monster 8.
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    Triggers::System system(level);
    Recorder rec;
    const auto handlers = rec.Handlers();
    const int cell = FirstCellWithTrigger(level, 12);
    REQUIRE(cell >= 0);
    int latch = 0;
    CHECK(system.OnShotEnterCell(latch, cell, handlers));
    CHECK_EQ(latch, 12);
    REQUIRE(rec.log.size() == 1);
    CHECK(rec.log[0] == "spawn 8+1");
    // The trigger had one use, so a second round finds it spent.
    int other = 0;
    CHECK(!system.OnShotEnterCell(other, cell, handlers));
}

TEST_CASE("Triggers: walk-only triggers ignore rounds")
{
    // L111 trigger 9 (the level exit) is flag 0x01 only.
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    Triggers::System system(level);
    Recorder rec;
    int latch = 0;
    CHECK(!system.OnShotEnterCell(latch, FirstCellWithTrigger(level, 9), rec.Handlers()));
    CHECK(rec.log.empty());
}

TEST_CASE("Triggers: a round skips switch commands and retries a failed chain")
{
    LevelGeometry level = SyntheticLevel({ { Triggers::ACTIVATE_OBJECT, 0, 1, 4 }, { Triggers::OPEN_DOOR, 0, 1, 0 } });
    level.actions[1].activationMask = Triggers::FLAG_SHOT;
    Triggers::System system(level);
    Recorder rec;
    rec.doorsSucceed = false;
    const auto handlers = rec.Handlers();
    int latch = 0;
    CHECK(!system.OnShotEnterCell(latch, 1, handlers));
    CHECK_EQ(latch, 0);                       // cleared, so the next tick tries again
    CHECK(rec.log.size() == 1 && rec.log[0] == "open 0+1");   // no "object 4"
    rec.doorsSucceed = true;
    CHECK(system.OnShotEnterCell(latch, 1, handlers));
    CHECK_EQ(latch, 1);
}

TEST_CASE("Triggers: Spawn Pickup reaches its handler and never fails the chain")
{
    LevelGeometry level = SyntheticLevel({ { Triggers::SPAWN_PICKUP, 0, 1, 6 }, { Triggers::END_LEVEL, 0, 0, 0 } });
    Triggers::System system(level);
    std::vector<std::pair<int, int>> spawned;
    bool ended = false;
    Triggers::Handlers h;
    h.spawnPickup = [&](int pickup, int amount) { spawned.push_back({ pickup, amount }); };
    h.endLevel = [&]() { ended = true; };
    CHECK(system.Run(1, Source::WalkOn, h));
    REQUIRE(spawned.size() == 1);
    CHECK_EQ(spawned[0].first, 6);
    CHECK_EQ(spawned[0].second, 1);
    CHECK(ended);
}
