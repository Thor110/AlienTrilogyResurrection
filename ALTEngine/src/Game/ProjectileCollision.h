#pragma once

#include "../Formats/LevelLoader.h"

#include <cstdint>

namespace ALTEngine::Game::ProjectileCollision
{
    // How a round in flight meets the world, transcribed from the original.
    //
    // Every tick the particle task (0x2d91c, a timer task Ghidra never made a
    // function of) moves every particle by its velocity, then for the player's
    // rounds (types 0-4) runs, in this order:
    //
    //   1. TestCell      FUN_0002a448 - walls, floor, ceiling, liquids and
    //                    level objects, all decided by the ONE cell the round
    //                    is now in. Blocked -> shatter, impact sound, gone.
    //   2. creatures     FUN_0002a628 - a per-creature box test, below.
    //                    Hit -> damage, a spray of the creature's blood, gone
    //                    (shotgun pellets and flame carry on through).
    //   3. triggers      FUN_000414f0 - a round entering a trigger cell can set
    //                    the trigger off (see Triggers::Source::Shot).
    //
    // NOTHING STEERS A ROUND. There is no aim assist anywhere on this path, nor
    // in the launch (FUN_0002b534, FUN_0002bcac): rounds fly where the view
    // points. What makes the original forgiving is the creature boxes - a
    // facehugger's reaches 512 above and below its middle.

    // Projectile types, the +0x1e byte (Screens/ParticleSystem.h has the rest).
    inline constexpr int TYPE_PISTOL = 0;
    inline constexpr int TYPE_SHOTGUN = 1;
    inline constexpr int TYPE_PULSE = 2;
    inline constexpr int TYPE_SMARTGUN = 3;
    inline constexpr int TYPE_FLAME = 4;
    inline constexpr int TYPE_GRENADE = 5;

    // ---- 1. the cell -------------------------------------------------------

    struct CellResult
    {
        bool blocked = false;
        // The level object the round struck and damaged (its index in
        // level.crates, as stamped into cell byte +4), or -1.
        int objectHit = -1;
        // What to take off that object's health.
        int damage = 0;
    };

    // FUN_0002a448 for a round at world height `y` in grid-space (gameX, gameZ).
    // `strength` is the round's +0x1a value (Particles::ProjectileDef::speed).
    //
    //   - cell attribute 0x14: solid to rounds.
    //   - an object stamped in the cell (+0x0c): 0x15 only takes flame, 0x1d
    //     ignores the smartgun and the round passes harmlessly but stops, 0x1c
    //     is hit at any height; any other object (> 0x13) is hit only between
    //     the floor and floor + 0x200 (0x400 for the tall 0x19). The pistol
    //     stops dead against a barrel (0x17) without hurting it.
    //   - liquids: attribute 9 stops rounds below floor + 0x180, 8 below
    //     floor + 0x100.
    //   - otherwise the round is clear only between the floor and the ceiling.
    //     Walls need no test of their own: every wall cell in the demo levels
    //     has its ceiling at or below its floor.
    //
    // Object damage is the round's strength as it stands - unlike creatures it
    // is never doubled for a fresh round.
    CellResult TestCell(const Formats::LevelGeometry& level, int gameX, int gameZ, float y,
                        int projectileType, int strength);

    // ---- 2. creatures ------------------------------------------------------

    // A creature's hit box, as half-sizes, set per type when it spawns
    // (FUN_0002e638 writes entity +0x08/+0x0a/+0x0c). Centred on the creature
    // in all three axes.
    struct HalfExtents
    {
        int x = 0x100, y = 0x200, z = 0x100;
    };
    HalfExtents CreatureExtents(int creatureType);

    // FUN_0002a628's box test. Shotgun pellets test against double-size boxes.
    bool OverlapsCreature(float px, float py, float pz, float cx, float cy, float cz,
                          int creatureType, int projectileType);

    // A creature can only be hit while its state (+0x6f) is below 6: from the
    // hit reaction (6) on, rounds pass straight through it.
    inline bool CreatureTargetable(int creatureState) { return creatureState < 6; }

    // The zone test that runs before the box: the creature's cell and the
    // round's cell must share at least one bit of their 16-bit zone mask (cell
    // bytes +8/+9). It stops a round in one room hitting a creature through
    // the wall in the next.
    uint16_t CellZones(const Formats::LevelGeometry& level, int cellIndex);
    bool ShareZone(const Formats::LevelGeometry& level, int cellA, int cellB);

    // Shotgun pellets and flame go on to hit every creature they overlap that
    // tick; every other round stops at the first.
    inline bool StopsAtFirstCreature(int projectileType)
    {
        return projectileType != TYPE_SHOTGUN && projectileType != TYPE_FLAME;
    }

    // The colour of blood a creature sprays when hit (FUN_0002af24): the
    // particle type FUN_0002ada4 spawns, or -1 for none. 9 is the aliens' acid.
    int BloodTypeForCreature(int creatureType);

    // ---- objects -----------------------------------------------------------

    // An object's starting health and whether it can be damaged at all, from
    // its level record (FUN_00037184): health is record byte 4 x 16, bit 0 of
    // byte 3 makes it indestructible, and switches and their kin (0x16, 0x18,
    // 0x1a, 0x1b, 0x20-0x25) always are.
    int ObjectHealth(const Formats::Crate& record);
    bool ObjectIndestructible(const Formats::Crate& record);

    // ---- acid blood --------------------------------------------------------

    // FUN_0002a8f8: a drop of acid blood (types 9 and 10) burns the player when
    // it falls within 0x100 of them across and between their feet and
    // feet + 0x400. The drop is used up when it does.
    bool AcidTouchesPlayer(float dropX, float dropY, float dropZ, float playerX, float playerFeetY, float playerZ);
    inline constexpr int ACID_DAMAGE_CAUSE = 5;
}
