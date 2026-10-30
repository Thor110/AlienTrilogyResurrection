#include "TestFramework.h"

#include "Formats/LevelLoader.h"
#include "Game/ProjectileCollision.h"
#include "Screens/EnemyBehaviour.h"
#include "Screens/ParticleSystem.h"
#include "Screens/PlayerCamera.h"
#include "Screens/PlayerHudState.h"
#include "Screens/SmartgunTargeting.h"

#include <cmath>

using namespace ALTEngine::Formats;
using namespace ALTEngine::Game;
namespace PCol = ALTEngine::Game::ProjectileCollision;

namespace
{
    // One open cell: floor 0, standard 0x30 ceiling (1536 world units).
    LevelGeometry OneCell()
    {
        LevelGeometry level;
        level.header.mapLength = 1;
        level.header.mapWidth = 1;
        level.collisionGrid.resize(1);
        level.collisionGrid[0].ceilingHeight = 0x30;
        level.collisionGrid[0].ceilingFog = 0x01;
        return level;
    }
}

TEST_CASE("ProjectileCollision: a round is clear between floor and ceiling")
{
    LevelGeometry level = OneCell();
    CHECK(!PCol::TestCell(level, 256, 256, 288.0f, PCol::TYPE_PISTOL, 0x10).blocked);
    CHECK(PCol::TestCell(level, 256, 256, -1.0f, PCol::TYPE_PISTOL, 0x10).blocked);
    CHECK(PCol::TestCell(level, 256, 256, 1537.0f, PCol::TYPE_PISTOL, 0x10).blocked);
    CHECK(PCol::TestCell(level, 600, 256, 288.0f, PCol::TYPE_PISTOL, 0x10).blocked);   // off the map
}

TEST_CASE("ProjectileCollision: every demo wall cell stops a round")
{
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    int walls = 0, nullSpace = 0;
    for (int cz = 0; cz < level.header.mapWidth; ++cz)
    {
        for (int cx = 0; cx < level.header.mapLength; ++cx)
        {
            const CollisionNode* cell = CellAt(level, cx, cz);
            if (cell->unknown3 != 255 && cell->unknown4 != 255) { continue; }
            if (cell->attribute == 0x14)
            {
                // A real wall: at any height.
                walls++;
                for (float y : { 1.0f, 288.0f, 800.0f, 1400.0f, 4000.0f })
                {
                    CHECK(PCol::TestCell(level, cx * 512 + 256, cz * 512 + 256, y, PCol::TYPE_PISTOL, 0x10).blocked);
                }
            }
            else
            {
                // Null space between the rooms (attribute 0, floor 0). The load
                // gives its ceiling of 0 a height of 0x30 (0x279a4), so like the
                // original it stops a round only below its floor or above that;
                // none of it borders a cell a round can be in.
                nullSpace++;
                const float ceiling = static_cast<float>(cell->ceilingHeight) * WORLD_UNITS_PER_HEIGHT_UNIT;
                CHECK(PCol::TestCell(level, cx * 512 + 256, cz * 512 + 256, ceiling + 1.0f, PCol::TYPE_PISTOL, 0x10).blocked);
            }
        }
    }
    CHECK(walls > 0);
    CHECK(nullSpace > 0);
}

TEST_CASE("ProjectileCollision: objects are hit inside their height window only")
{
    LevelGeometry level = OneCell();
    level.collisionGrid[0].unknown13 = 0x14;   // a crate
    level.collisionGrid[0].unknown5 = 7;       // record 7
    auto low = PCol::TestCell(level, 256, 256, 288.0f, PCol::TYPE_PISTOL, 0x10);
    CHECK(low.blocked);
    CHECK_EQ(low.objectHit, 7);
    CHECK_EQ(low.damage, 0x10);   // not doubled, unlike creatures
    auto high = PCol::TestCell(level, 256, 256, 700.0f, PCol::TYPE_PISTOL, 0x10);
    CHECK(!high.blocked);         // over the top of it

    level.collisionGrid[0].unknown13 = 0x19;   // the tall one: 0x400
    CHECK(PCol::TestCell(level, 256, 256, 700.0f, PCol::TYPE_PISTOL, 0x10).objectHit == 7);
}

TEST_CASE("ProjectileCollision: the pistol stops against a barrel without hurting it")
{
    LevelGeometry level = OneCell();
    level.collisionGrid[0].unknown13 = 0x17;
    level.collisionGrid[0].unknown5 = 3;
    auto pistol = PCol::TestCell(level, 256, 256, 200.0f, PCol::TYPE_PISTOL, 0x10);
    CHECK(pistol.blocked);
    CHECK_EQ(pistol.objectHit, -1);
    CHECK_EQ(PCol::TestCell(level, 256, 256, 200.0f, PCol::TYPE_SHOTGUN, 0x50).objectHit, 3);
}

TEST_CASE("ProjectileCollision: 0x15 objects only take smartgun rounds")
{
    LevelGeometry level = OneCell();
    level.collisionGrid[0].unknown13 = 0x15;
    level.collisionGrid[0].unknown5 = 1;
    CHECK_EQ(PCol::TestCell(level, 256, 256, 200.0f, PCol::TYPE_PULSE, 0x60).objectHit, -1);
    CHECK_EQ(PCol::TestCell(level, 256, 256, 200.0f, PCol::TYPE_SMARTGUN, 0xa0).objectHit, 1);
}

TEST_CASE("ProjectileCollision: liquids stop rounds near their surface")
{
    LevelGeometry level = OneCell();
    level.collisionGrid[0].attribute = 9;
    CHECK(PCol::TestCell(level, 256, 256, 0x17f, PCol::TYPE_PISTOL, 0x10).blocked);
    CHECK(!PCol::TestCell(level, 256, 256, 0x181, PCol::TYPE_PISTOL, 0x10).blocked);
}

TEST_CASE("ProjectileCollision: creature boxes are per type and centred")
{
    // A facehugger (type 2) standing at floor + 0x20 is 512 high either side.
    CHECK(PCol::OverlapsCreature(0, 288, 0, 0, 32, 0, 2, PCol::TYPE_PISTOL));
    CHECK(PCol::OverlapsCreature(0, 32 + 511, 0, 0, 32, 0, 2, PCol::TYPE_PISTOL));
    CHECK(!PCol::OverlapsCreature(0, 32 + 513, 0, 0, 32, 0, 2, PCol::TYPE_PISTOL));
    CHECK(!PCol::OverlapsCreature(257, 288, 0, 0, 32, 0, 2, PCol::TYPE_PISTOL));
    CHECK(PCol::OverlapsCreature(257, 288, 0, 0, 32, 0, 2, PCol::TYPE_SHOTGUN));   // doubled
    CHECK_EQ(PCol::CreatureExtents(7).y, 0x600);
    CHECK_EQ(PCol::CreatureExtents(5).y, 0x300);
}

TEST_CASE("ProjectileCollision: a hugger on the floor needs the look keys, a leaping one does not")
{
    // The original's numbers end to end: eye at floor + 0x300, rounds 0x60
    // lower. At the rest pitch (slightly down) a round passes over a
    // facehugger's box - its top is floor + 0x20 + 0x200 - but holding Look
    // Down (pitch -0x80) brings it in, while a warrior's taller box (0x300)
    // takes level fire.
    namespace PC = ALTEngine::Screens::PlayerCamera;
    const float launchY = PC::EYE_HEIGHT_ORIGINAL - ALTEngine::Screens::Particles::MUZZLE_DROP;
    auto yAt = [&](int pitch, float distance) {
        return launchY + distance * std::tan(pitch * 6.28318530718f / PC::ANGLE_UNITS);
    };
    const float range = 1500.0f;
    CHECK(!PCol::OverlapsCreature(0, yAt(PC::PITCH_REST, range), range, 0, 32, range, 2, PCol::TYPE_PISTOL));
    CHECK(PCol::OverlapsCreature(0, yAt(PC::PITCH_MIN, range), range, 0, 32, range, 2, PCol::TYPE_PISTOL));
    CHECK(PCol::OverlapsCreature(0, yAt(PC::PITCH_REST, range), range, 0, 32, range, 5, PCol::TYPE_PISTOL));

    // But a hugger does not stay on the floor: it attacks by leaping
    // (FUN_00033cbc, vertical velocity 0x80 less 0xc a tick), and for most of
    // the leap level fire passes through its box. That is why the original
    // never needed the look keys to shoot one.
    namespace EB = ALTEngine::Screens::EnemyBehaviour;
    float y = EB::STAND_CLEARANCE, vy = EB::LEAP_LIFT;
    int hittableTicks = 0, airborneTicks = 0;
    while (vy != 0.0f)
    {
        const float next = y + vy;
        if (next < EB::STAND_CLEARANCE) { break; }
        y = next;
        vy -= EB::GRAVITY;
        if (vy == 0.0f) { vy = -EB::GRAVITY; }
        ++airborneTicks;
        if (PCol::OverlapsCreature(0, yAt(PC::PITCH_REST, range), range, 0, y, range, 2, PCol::TYPE_PISTOL)) { ++hittableTicks; }
    }
    CHECK(airborneTicks > 15);
    CHECK(hittableTicks * 2 > airborneTicks);
}

TEST_CASE("ProjectileCollision: zones and targetability")
{
    LevelGeometry level;
    level.header.mapLength = 3;
    level.header.mapWidth = 1;
    level.collisionGrid.resize(3);
    level.collisionGrid[0].ceilingFog = 0x01;
    level.collisionGrid[1].ceilingFog = 0x03;
    level.collisionGrid[2].floorFog = 0x01;   // bit 8
    CHECK(PCol::ShareZone(level, 0, 1));
    CHECK(!PCol::ShareZone(level, 0, 2));
    CHECK(PCol::CreatureTargetable(0));
    CHECK(PCol::CreatureTargetable(5));
    CHECK(!PCol::CreatureTargetable(6));
    CHECK(!PCol::StopsAtFirstCreature(PCol::TYPE_SHOTGUN));
    CHECK(PCol::StopsAtFirstCreature(PCol::TYPE_PISTOL));
}

TEST_CASE("ProjectileCollision: object health and indestructibility come from the record")
{
    Crate crate;
    crate.type = 0x14;
    crate.healthDiv16 = 5;
    CHECK_EQ(PCol::ObjectHealth(crate), 80);
    CHECK(!PCol::ObjectIndestructible(crate));
    crate.type = 0x16;   // a switch
    CHECK(PCol::ObjectIndestructible(crate));
    crate.type = 0x14;
    crate.flags = 1;
    CHECK(PCol::ObjectIndestructible(crate));
}

TEST_CASE("ProjectileCollision: acid burns only close to the player")
{
    CHECK(PCol::AcidTouchesPlayer(100, 50, -100, 0, 0, 0));
    CHECK(!PCol::AcidTouchesPlayer(300, 50, 0, 0, 0, 0));
    CHECK(!PCol::AcidTouchesPlayer(0, -10, 0, 0, 0, 0));
    CHECK(!PCol::AcidTouchesPlayer(0, 0x401, 0, 0, 0, 0));
    CHECK_EQ(PCol::BloodTypeForCreature(2), 9);
    CHECK_EQ(PCol::BloodTypeForCreature(11), 8);
}

TEST_CASE("PlayerHudState: armour takes the whole hit, then health, with a cooldown")
{
    ALTEngine::Screens::PlayerHudState hud;
    hud.armourItem = 10;
    CHECK(hud.ApplyDamage(25, 4));
    CHECK_EQ(hud.armourItem, 0);
    CHECK_EQ(hud.health, 100);       // armour soaked all 25
    CHECK(!hud.ApplyDamage(5, 4));   // cooldown
    for (int i = 0; i < 0x1e; ++i) { hud.Tick(); }
    CHECK(hud.ApplyDamage(5, 4));
    CHECK_EQ(hud.health, 95);
    hud.damageCooldown = 0;
    hud.terrainShield = 10;
    CHECK(!hud.ApplyDamage(50, 5));  // the terrain shield blocks everything
    hud.terrainShield = 0;
    hud.health = 3;
    CHECK(hud.ApplyDamage(5, 5));
    CHECK(hud.dead);
    CHECK_EQ(hud.lastDamageCause, 5);
}

TEST_CASE("PlayerHudState: floor hazards and pits")
{
    ALTEngine::Screens::PlayerHudState hud;
    hud.ApplyFloorHazard(8);            // acid: 10
    CHECK_EQ(hud.health, 90);
    hud.damageCooldown = 0;
    hud.terrainShield = 50;
    hud.ApplyFloorHazard(7);            // shielded: nothing
    CHECK_EQ(hud.health, 90);
    hud.armourItem = 100;
    hud.ApplyFloorHazard(10);           // a pit: through the shield and the armour
    CHECK(hud.dead);
    CHECK_EQ(hud.armourItem, 100);
}

TEST_CASE("Particles: pistol rounds converge on the line of sight")
{
    // FUN_0002b534 starts a pistol round 0x28 to the side and yaws it 0x28
    // back across, so it crosses the line of sight about 650 units out.
    namespace P = ALTEngine::Screens::Particles;
    P::Pool pool;
    P::Particle* round = pool.SpawnProjectile(P::TYPE_PISTOL, 0, 0x300, 0, 0, 0, 0);
    REQUIRE(round != nullptr);
    const float side0 = round->x;          // yaw 0: the side axis is x
    CHECK(std::fabs(side0) > 30.0f);
    CHECK(round->vx * side0 < 0.0f);       // heading back toward the middle
    const float crossing = -side0 / round->vx * std::fabs(round->vz);
    CHECK(crossing > 550.0f);
    CHECK(crossing < 750.0f);
}

TEST_CASE("Smartgun: locks on to creatures in the cone, nearest first")
{
    namespace SGT = ALTEngine::Screens::SmartgunTargeting;
    // Player at the origin facing yaw 0, which is -z in the port.
    std::vector<SGT::Creature> creatures{
        { 0, -3000, 2, 0, 3000, true },     // dead ahead, far
        { 0, -1000, 2, 0, 1000, true },     // dead ahead, near
        { 2000, -2000, 2, 0, 2828, true },  // 45 degrees off: outside the cone
        { 0, -9000, 2, 0, 9000, true },     // beyond 0x1c00
        { 0, -500, 2, 6, 500, true },       // hit/dying: not a target
        { 0, -600, 2, 0, 600, false },      // not visible
        { 100, -2000, 2, 0, 2002, true },   // about 3 degrees off: inside
    };
    const auto targets = SGT::FindTargets(creatures, 0, 0, 0);
    REQUIRE(targets.size() == 3);
    CHECK_EQ(targets[0].index, 1);
    CHECK_EQ(targets[1].index, 6);
    CHECK_EQ(targets[2].index, 0);
    CHECK(targets[1].offset != 0);
}

TEST_CASE("Smartgun: an aimed round flies at its target")
{
    namespace SGT = ALTEngine::Screens::SmartgunTargeting;
    const SGT::Vec v = SGT::AimVelocity(0, 0x200, 0, 0, 0x120, -2000, 0x100);
    CHECK(v.y < 0.0f);                       // down toward a creature on the floor
    CHECK(v.z < 0.0f);
    CHECK(std::fabs(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z) - 0x100) < 0.5f);
    // Following it lands on the target.
    const float t = -2000.0f / v.z;
    CHECK(std::fabs(0x200 + v.y * t - 0x120) < 1.0f);
}

TEST_CASE("Particles: a flame stops where it is blocked and burns on")
{
    namespace P = ALTEngine::Screens::Particles;
    P::Pool pool;
    P::Particle* flame = pool.SpawnProjectile(P::TYPE_FLAME, 0, 0x300, 0, 0, 0, 0);
    REQUIRE(flame != nullptr);
    const int lifeBefore = flame->life;
    pool.Tick([](float, float, float, int, int, float, float, float, int&) { return 1; });
    CHECK(flame->alive);
    CHECK_EQ(flame->vx, 0.0f);
    CHECK_EQ(flame->vz, 0.0f);
    CHECK_EQ(flame->life, lifeBefore - 1);

    // A pistol round in the same place is gone.
    P::Particle* round = pool.SpawnProjectile(P::TYPE_PISTOL, 0, 0x300, 0, 0, 0, 0);
    REQUIRE(round != nullptr);
    pool.Tick([](float, float, float, int, int, float, float, float, int&) { return 2; });
    CHECK(!round->alive);
}

TEST_CASE("Particles: a flame's spawn steps go through the collision test, and stop where it hits")
{
    namespace P = ALTEngine::Screens::Particles;
    P::Pool pool(4);
    P::Particle* flame = pool.SpawnProjectile(P::TYPE_FLAME, 0, 0x300, 0, 0, 0, 0);
    REQUIRE(flame);
    flame->vx = 100; flame->vy = 0; flame->vz = 0;
    flame->life = 0x0e;
    flame->preSteps = 2;
    const float startX = flame->x;
    int tests = 0;
    // Clear for the first step, something there on the second (a crate, say).
    pool.Tick([&](float x, float, float, int, int, float, float, float, int&) {
        ++tests;
        return x > startX + 150.0f ? 1 : 0;
    });
    CHECK_EQ(tests, 2);
    CHECK(flame->alive);
    CHECK_EQ(flame->x, startX + 100.0f); // stopped after the clear step
    CHECK_EQ(flame->vx, 0.0f);           // and burns there
    CHECK_EQ(flame->life, 0x0e);         // the spawn steps take no life
    CHECK_EQ(flame->preSteps, 0);
}
