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

    // FUN_00033a1c's first test: the creature's cell must share a zone bit with
    // what the player can see (DAT_000b0cf0, which the renderer builds from
    // every cell it draws). The port stands in the zones of the player's own
    // cell - the same rule rounds use - since it does not walk visible cells.
    bool ZoneVisible(const Formats::LevelGeometry& level, int creatureCell, int playerCell);
}
