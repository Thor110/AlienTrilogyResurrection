#include "TestFramework.h"

#include "Game/SaveGame.h"
#include "Screens/LevelSnapshot.h"

#include <filesystem>

namespace LS = ALTEngine::Screens::LevelSnapshot;
namespace EN = ALTEngine::Screens::Enemies;
using ALTEngine::Formats::CollisionNode;

TEST_CASE("Save Anywhere: the cell grid comes back byte for byte")
{
    std::vector<CollisionNode> grid(3);
    grid[0].floorHeight = 12; grid[0].ceilingHeight = 40; grid[0].unknown13 = 0x15;
    grid[1].attribute = 0x19; grid[1].scriptAction = 7; grid[1].lighting = 0xff;
    grid[2].unknown1 = 0xab; grid[2].floorFog = 3;
    const std::string hex = LS::GridToHex(grid);
    CHECK_EQ(hex.size(), 3u * 32);

    std::vector<CollisionNode> back(3);
    REQUIRE(LS::GridFromHex(back, hex));
    CHECK_EQ(back[0].floorHeight, 12);
    CHECK_EQ(back[0].ceilingHeight, 40);
    CHECK_EQ(back[0].unknown13, 0x15);
    CHECK_EQ(back[1].attribute, 0x19);
    CHECK_EQ(back[1].scriptAction, 7);
    CHECK_EQ(back[1].lighting, 0xff);
    CHECK_EQ(back[2].unknown1, 0xab);
    CHECK_EQ(back[2].floorFog, 3);

    // Another level's grid is refused, and leaves this one alone.
    std::vector<CollisionNode> other(2);
    CHECK(!LS::GridFromHex(other, hex));
    CHECK_EQ(other[0].floorHeight, 0);
}

TEST_CASE("Save Anywhere: doors and lifts keep where they had got to")
{
    ALTEngine::Game::DoorState door;
    door.travel = 30;
    door.progress = 17;
    door.phase = ALTEngine::Game::DoorState::Phase::Opening;
    door.switchOperated = true;
    door.unlockProgress = 2;
    ALTEngine::Game::DoorState back;
    back.travel = 30;
    LS::LoadDoor(back, LS::SaveDoor(door));
    CHECK_EQ(back.progress, 17);
    CHECK(back.phase == ALTEngine::Game::DoorState::Phase::Opening);
    CHECK(back.switchOperated);
    CHECK_EQ(back.unlockProgress, 2);

    ALTEngine::Game::Lifts::Lift lift;
    lift.height = 9; lift.counter = 2; lift.flags = 0x81;
    lift.phase = ALTEngine::Game::Lifts::Phase::Ascend;
    ALTEngine::Game::Lifts::Lift liftBack;
    LS::LoadLift(liftBack, LS::SaveLift(lift));
    CHECK_EQ(liftBack.height, 9);
    CHECK_EQ(liftBack.counter, 2);
    CHECK_EQ(liftBack.flags, 0x81);
    CHECK(liftBack.phase == ALTEngine::Game::Lifts::Phase::Ascend);
}

TEST_CASE("Save Anywhere: a creature comes back where it was, and decides afresh")
{
    EN::Enemy warrior;
    warrior.type = 6; warrior.active = true; warrior.alive = true;
    warrior.x = 1200.5f; warrior.y = 64.0f; warrior.z = -300.0f;
    warrior.facing = 0x400; warrior.health = 200;
    warrior.animation = 2;   // a walk view
    warrior.ai.alerted = true;
    warrior.ai.state = ALTEngine::Screens::CreatureAI::ACT;
    warrior.leaping = true;
    warrior.state = ALTEngine::Screens::EnemyBehaviour::STATE_HIT_RECOVER;

    EN::Enemy back;
    back.type = 6;
    LS::LoadEnemy(back, LS::SaveEnemy(warrior));
    CHECK(back.active);
    CHECK(back.alive);
    CHECK_EQ(back.x, 1200.5f);
    CHECK_EQ(back.z, -300.0f);
    CHECK_EQ(back.facing, 0x400);
    CHECK_EQ(back.health, 200);
    CHECK(back.ai.alerted);
    CHECK_EQ(back.ai.state, ALTEngine::Screens::CreatureAI::IDLE);   // the attack is dropped
    CHECK(!back.leaping);
    CHECK_EQ(back.state, ALTEngine::Screens::EnemyBehaviour::STATE_ACTING);
    CHECK(back.sequence.size() > 0);   // its animation script started again

    // One dying when saved is dead when loaded.
    EN::Enemy dying = warrior;
    dying.state = ALTEngine::Screens::EnemyBehaviour::STATE_DYING;
    EN::Enemy dead;
    dead.type = 6;
    LS::LoadEnemy(dead, LS::SaveEnemy(dying));
    CHECK(!dead.alive);
}

TEST_CASE("Save Anywhere: an opened egg stays open")
{
    EN::Enemy egg;
    egg.type = 1; egg.active = true;
    egg.egg.state = ALTEngine::Screens::CreatureTasks::Egg::OPEN;
    egg.egg.phase = ALTEngine::Screens::CreatureTasks::Egg::PHASE_OPEN;
    egg.animation = ALTEngine::Screens::CreatureTasks::Egg::ANIM_OPENING;
    egg.animationFrame = 7;
    EN::Enemy back;
    back.type = 1;
    LS::LoadEnemy(back, LS::SaveEnemy(egg));
    CHECK_EQ(back.egg.state, ALTEngine::Screens::CreatureTasks::Egg::OPEN);
    CHECK_EQ(back.egg.phase, ALTEngine::Screens::CreatureTasks::Egg::PHASE_OPEN);
    CHECK_EQ(back.animation, ALTEngine::Screens::CreatureTasks::Egg::ANIM_OPENING);
    CHECK_EQ(back.animationFrame, 7);
}

TEST_CASE("Save Anywhere: the player's health, items and view come back")
{
    ALTEngine::Screens::PlayerHudState hud;
    hud.health = 37; hud.grenades = 4; hud.armourItem = 50; hud.terrainShield = 99;
    ALTEngine::Screens::PlayerHudState hudBack;
    LS::LoadHud(hudBack, LS::SaveHud(hud));
    CHECK_EQ(hudBack.health, 37);
    CHECK_EQ(hudBack.grenades, 4);
    CHECK_EQ(hudBack.armourItem, 50);
    CHECK_EQ(hudBack.terrainShield, 99);

    ALTEngine::Screens::PlayerCamera::State cam;
    cam.yaw = 0x123; cam.pitch = -0x40; cam.speed = 0x78;
    ALTEngine::Screens::PlayerCamera::State camBack;
    LS::LoadCamera(camBack, LS::SaveCamera(cam));
    CHECK_EQ(camBack.yaw, 0x123);
    CHECK_EQ(camBack.pitch, -0x40);
    CHECK_EQ(camBack.speed, 0);   // standing still on load
}

TEST_CASE("Save Anywhere: a save file keeps its snapshot")
{
    const auto dir = std::filesystem::temp_directory_path() / "alt_snapshot_test";
    std::filesystem::remove_all(dir);
    ALTEngine::Game::SaveData data;
    data.name = "MISSION 1.1.1";
    data.levelDigits = "111";
    data.snapshot = R"({"version":1,"level":"111","grid":"00"})";
    REQUIRE(ALTEngine::Game::SaveGame::Write(dir, 3, data));
    const auto back = ALTEngine::Game::SaveGame::Read(dir, 3);
    REQUIRE(back.has_value());
    const auto parsed = nlohmann::json::parse(back->snapshot);
    CHECK(parsed.value("level", std::string{}) == "111");
    CHECK_EQ(parsed.value("version", 0), 1);

    // A level-start checkpoint has none.
    data.snapshot.clear();
    REQUIRE(ALTEngine::Game::SaveGame::Write(dir, 4, data));
    CHECK(ALTEngine::Game::SaveGame::Read(dir, 4)->snapshot.empty());
    std::filesystem::remove_all(dir);
}
