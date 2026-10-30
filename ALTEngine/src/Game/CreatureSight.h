#pragma once

#include "../Formats/LevelLoader.h"

#include <cstdint>
#include <vector>

namespace ALTEngine::Game::CreatureSight
{
    // CAN THIS CREATURE REACH THE PLAYER IN A STRAIGHT LINE - FUN_0003231c.
    //
    // The creature task asks this before every attack (and before deciding
    // to make one): a creature that cannot get a clear line to the player does
    // not attack, whatever its range.
    //
    // It walks TWO parallel probes from the creature toward the player, 200
    // units either side of the line between them (the 0x400 and 0xc00 turns
    // of the bearing), 0x100 units a step, for distance >> 8 steps (at least
    // two). At each probe point, in turn, the line is refused by:
    //   - a cell off the map;
    //   - a wall (attribute 0x14-0x2c) or an occupied cell (+0x0c non-zero -
    //     the creature's own stamp is lifted first);
    //   - for cells below 0x2d: a ceiling lower than the running height plus
    //     the creature's half height; for type 7, one less than 0xc00 above
    //     it; a rise of more than 0x100 (0x200 for a ceiling crawler) or a
    //     drop of more than 0x400 (0x800);
    //   - for ramp cells (0x2d and up) only the type 7 ceiling test.
    // The running height starts at the creature's own Y and follows the floor
    // (the ceiling, for a crawler) of each cell it passes, shared by both
    // probes. Beyond 0x2000 the answer is no without looking.
    //
    // All positions are in grid space (FindFloorHeightGridSpace's).
    inline constexpr int MAX_DISTANCE = 0x2000;
    inline constexpr float PROBE_SIDE = 200.0f;
    inline constexpr float PROBE_STEP = 0x100;

    struct Query
    {
        float x = 0, y = 0, z = 0;           // the creature
        float targetX = 0, targetZ = 0;      // the player
        int distance = 0;                    // the creature's own +0x60
        int halfHeight = 0x200;              // its box, +0x0a
        bool ceilingCrawler = false;         // +0x6c bit 8
        bool tallType = false;               // type 7
        int occupiedCellX = -1, occupiedCellZ = -1;   // the player's cell (0xff stamp)

        // Every cell's occupancy (+0x0c), one byte per cell, if the caller
        // keeps one: the player's mark and every creature's. When given, it
        // replaces occupiedCellX/Z, and the caller is expected to have lifted
        // this creature's own mark first, as FUN_0003231c does.
        const std::vector<uint8_t>* occupancy = nullptr;
    };

    bool LineOfFire(const Formats::LevelGeometry& level, const Query& query);

    // THE GUNNERS' AND THE EGG'S LINE - FUN_00032aec, which types 0x0b-0x0e
    // use in place of FUN_0003231c throughout the common task (0x344c5,
    // 0x3463b, 0x34998 and the rest test the type first) and the egg uses to
    // decide to open (0x357af).
    //
    // A single ray in three dimensions, not two probes on the ground: from
    // 0x240 above the creature to the player's eye, 0x400 above the player's
    // floor, pitched by the angle between them (FUN_00026c60's distance and
    // FUN_00026b64's angle) and set off along the bearing to the player (+0x54), 0x100 a
    // step for distance >> 8 steps (at least two). At each point it is refused
    // by a cell off the map, a wall (attribute 0x14-0x2c), a floor above it or
    // a ceiling below it - and what stands in the cell raises the floor rather
    // than blocking: a crate (0x14) or a barrel (0x17) by 0x200, a double crate
    // (0x19) by 0x400. Anything else there blocks (a creature, the player, a
    // switch). Beyond 0x1555 the answer is no without looking.
    inline constexpr int MAX_DISTANCE_3D = 0x1555;
    inline constexpr int RAY_START_HEIGHT = 0x240;
    inline constexpr int RAY_TARGET_HEIGHT = 0x400;

    struct Query3D
    {
        float x = 0, y = 0, z = 0;           // the creature
        int heading = 0;                     // +0x54: the bearing to the player
        float targetX = 0, targetZ = 0;      // the player
        float targetFloorY = 0;              // the player's floor (b0a4a)
        int distance = 0;                    // the creature's own +0x60
        // Creature and player marks, as for Query; objects are read from the
        // cells' own +0x0c (CollisionNode::unknown13).
        const std::vector<uint8_t>* occupancy = nullptr;
    };

    bool LineOfFire3D(const Formats::LevelGeometry& level, const Query3D& query);

    // The zones the renderer drew this frame (DAT_000b0cf2, built at
    // 0x2745b). It starts as the zones of the player's own cell; then the
    // cells round the player are walked ring by ring out to 15 (ViewRings.h),
    // within 0x600 of the view yaw for the first ring, 0x384 for the second,
    // 0x280 to the eighth and 0x1b8 beyond. A cell sharing a zone with the mask
    // so far is drawn, and if it has faces its zones join the mask - so the
    // mask spreads through the zones the player can actually see into.
    // `viewYaw` is the camera's (DAT_000b0ba2).
    uint16_t DrawnZoneMask(const Formats::LevelGeometry& level, int playerCellX, int playerCellZ, int viewYaw);

    // The cells the renderer draws this frame (0x2745b): the player's own and
    // every cell the walk takes in, one byte a cell. `minHalfWidth` widens
    // each pass's cone to at least that many angle units either side of the
    // view, for a view wider than the original's 320x240 (whose cones it
    // already covers); 0 keeps the original's cones.
    std::vector<uint8_t> DrawnCells(const Formats::LevelGeometry& level, int playerCellX, int playerCellZ,
                                    int viewYaw, int minHalfWidth = 0);

    // Which cell each level face belongs to: a cell's bytes 0-3 give the
    // first face of its run, counting from 1 (record 0 is the blank before
    // face 0), and the run ends at the face whose light byte (+0x13) has bit
    // 0x80. -1 for a face no cell lists.
    std::vector<int32_t> FaceCells(const Formats::LevelGeometry& level);

    // What the same walk leaves behind for the map. Every cell it takes in -
    // the player's own and each ring cell sharing a zone with the mask - gets
    // its seen bit (byte 7, bit 0: 0x274d8, 0x275b8 and the other passes),
    // and the area of the player's cell (byte 5) is marked discovered
    // (DAT_00245bb4, 0x274e1). Area 0 is discovered from the start (0x28339).
    // `seen` is one byte a cell (mapLength stride), `areas` 256 bytes; both are
    // sized here if they need it.
    void MarkSeen(const Formats::LevelGeometry& level, std::vector<uint8_t>& seen, std::vector<uint8_t>& areas,
                  int playerCellX, int playerCellZ, int viewYaw);

    // Which cells the map shows (FUN_00043078): a cell of a discovered area
    // that has been seen, or with the Auto Mapper any cell of a discovered
    // area. One byte a cell, non-zero to show.
    std::vector<uint8_t> MapReveal(const Formats::LevelGeometry& level, const std::vector<uint8_t>& seen,
                                   const std::vector<uint8_t>& areas, bool autoMapper);

    // FUN_00033a1c's first test: the creature's cell shares a zone with that
    // mask.
    bool ZoneVisible(const Formats::LevelGeometry& level, int creatureCell, uint16_t drawnZones);
}
