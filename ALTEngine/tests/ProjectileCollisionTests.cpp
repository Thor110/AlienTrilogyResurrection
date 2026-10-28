#include "TestFramework.h"

#include "Formats/LevelLoader.h"
#include "Game/ProjectileCollision.h"
#include "Screens/ParticleSystem.h"
#include "Screens/PlayerCamera.h"
#include "Screens/PlayerHudState.h"

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

TEST_CASE("ProjectileCollision: every demo wall cell stops a round at any height")
{
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    int walls = 0;
    for (int cz = 0; cz < level.header.mapWidth; ++cz)
    {
        for (int cx = 0; cx < level.header.mapLength; ++cx)
        {
            const CollisionNode* cell = CellAt(level, cx, cz);
            if (cell->unknown3 != 255 && cell->unknown4 != 255) { continue; }
            walls++;
            // Not y == 0 exactly: with floor and ceiling both 0 the original's
            // own `y < floor` / `y <= ceiling` pair lets that one height through.
            for (float y : { 1.0f, 288.0f, 800.0f, 1400.0f })
            {
                CHECK(PCol::TestCell(level, cx * 512 + 256, cz * 512 + 256, y, PCol::TYPE_PISTOL, 0x10).blocked);
            }
        }
    }
    CHECK(walls > 0);
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

TEST_CASE("ProjectileCollision: 0x15 objects only take flame")
{
    LevelGeometry level = OneCell();
    level.collisionGrid[0].unknown13 = 0x15;
    level.collisionGrid[0].unknown5 = 1;
    CHECK_EQ(PCol::TestCell(level, 256, 256, 200.0f, PCol::TYPE_PULSE, 0x60).objectHit, -1);
    CHECK_EQ(PCol::TestCell(level, 256, 256, 200.0f, PCol::TYPE_FLAME, 0xa0).objectHit, 1);
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

TEST_CASE("ProjectileCollision: a level shot from the original eye height reaches a facehugger")
{
    // The original's numbers end to end: eye at floor + 0x180, rounds 0x60 lower
    // and climbing at the rest pitch 0x30. A facehugger 3000 units away on
    // level ground is still inside its box when the round gets there.
    namespace PC = ALTEngine::Screens::PlayerCamera;
    const float launchY = PC::EYE_HEIGHT_ORIGINAL - ALTEngine::Screens::Particles::MUZZLE_DROP;
    const float climb = std::tan(PC::PITCH_REST * 6.28318530718f / PC::ANGLE_UNITS);
    const float yAt3000 = launchY + 3000.0f * climb;
    CHECK(PCol::OverlapsCreature(0, yAt3000, 3000, 0, 32, 3000, 2, PCol::TYPE_PISTOL));
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
    crate.unknown1 = 5;
    CHECK_EQ(PCol::ObjectHealth(crate), 80);
    CHECK(!PCol::ObjectIndestructible(crate));
    crate.type = 0x16;   // a switch
    CHECK(PCol::ObjectIndestructible(crate));
    crate.type = 0x14;
    crate.drop = 1;
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
