#pragma once

#include "../Formats/LevelLoader.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace ALTEngine::Renderer
{
    // Top-down map of a level, drawn from the collision grid.
    //
    // THE ORIGINAL'S MAP RENDERER IS FUN_00043cc4 - found, and it works quite
    // differently from this:
    //
    //   origin  = (0xe0 - gridWidth/2, 0x60 - gridHeight/2) in 320x240 space,
    //             i.e. the map is CENTRED on (224, 96)
    //   extent  = origin .. origin + gridWidth-1 / gridHeight-1
    //             so ONE PIXEL PER CELL, not a scaled block per cell
    //   player  = a 3x3 point, colour ff,7f,00 (orange), at
    //             origin + ((worldPos + offset) >> 9) - 1, entry stride 0x14
    //   the map itself = a SINGLE TEXTURED QUAD with UVs from DAT_00249830.., and
    //             a modulate colour of 0x4c,0x73,0x4c = RGB(76,115,76)
    //
    // So the original builds the map into a TEXTURE somewhere else and draws it as
    // one quad, tinted that muted green - it does not emit a rect per cell the way
    // this does. There is also a second mode when DAT_0024985c == 2, which draws a
    // fixed 0x74 (116) square at (0xa6, 0x26) = (166, 38) with UV clamping either
    // side - almost certainly the live HUD version, scrolling a window over the
    // same texture.
    //
    // THE TEXTURE BUILDER IS FUN_00043078 - found, via the writes to DAT_00249830
    // (which is the map quad's UV pair, not a colour). It walks the grid 0..0x7f in
    // both axes and writes ONE BYTE PER CELL into the texture, from a set of just
    // four values:
    //
    //     0   outside the grid, or a cell with nothing to show
    //     1   walkable
    //     2   wall
    //     4   written when bit 0x80 of DAT_000b0ab4 is set, which takes a separate
    //         branch for every cell - almost certainly the Auto Mapper's
    //         reveal-everything path
    //
    // Note 3 is never written. So the map is an 8bpp indexed image and the actual
    // COLOURS live in that texture's CLUT, which is uploaded elsewhere - it is not
    // in this function and not in the panel sheet's CL00. That CLUT is the last
    // piece needed to get the colours exactly right.
    //
    // *** AND THE ORIGINAL'S VISITED FLAG IS BYTE +7, BIT 1 OF THE RUNTIME
    // COLLISION CELL. *** FUN_00043078 tests `cell[7] & 1` to decide whether a cell
    // has been seen, and bit 0x80 of the same byte marks something further. This is
    // the tracking I looked for and could not find when the fog of war was written
    // - it is stored in the collision grid itself, not a separate buffer. Worth
    // adopting in place of this port's own `visited` vector, since it would then be
    // whatever the original actually reveals rather than an approximation.
    //
    // THE FULL PER-CELL LOGIC, traced from FUN_00043078 (see Minimap.cpp for how it
    // maps onto the on-disc fields). Five values, not three:
    //
    //     0  not drawn        4  obstacle
    //     1  walkable         5  VENT - byte +10 == 6
    //     2  wall
    //
    // Verified against L111: 5774 walls, 3861 walkable, and 24 VENTS - which is the
    // feature Edward identified as facehugger spawn points, drawn by the original as
    // its own box rather than a bump in the wall. Nothing would have found those by
    // guesswork; they only appear because byte +10 == 6 is tested explicitly.
    //
    // TWO THINGS THIS PORT STILL CANNOT REPRODUCE:
    //
    //  1. DAT_00245bb4 - a 256-entry byte table indexed by cell byte +5. When its
    //     entry is zero the cell is NOT DRAWN AT ALL. This is the real null-space
    //     test, and it replaces the neighbour heuristic that was here before. On
    //     L111 byte +5 is 0 for every cell, so the table's entry 0 alone decides
    //     whether empty cells appear - but other levels will use more of it.
    //  2. Byte +12, which selects the obstacle value 4, is 0 for every cell on disc.
    //     So obstacles are marked at RUNTIME, presumably when a crate is placed -
    //     which is why this port draws them from the crate records instead.
    //
    // Given the CLUT is still missing, the per-cell approach below stays for now. Two things from the above are worth adopting regardless
    // and have been: the modulate colour, and one pixel per cell.
    //
    // NOT TRACED FROM THE ORIGINAL. Nothing resembling a map or motion-tracker
    // renderer has been located in the decompilation - there is no function
    // that walks the grid and draws it, and no automap symbol of any kind. So
    // the DATA here is the game's own (the collision grid, the door and lift
    // records, the player position) but the styling is this port's invention.
    // If the original's renderer turns up later, expect the look to change;
    // nothing else depends on how this draws.
    //
    // What each cell is taken from:
    //   unknown3 / unknown4 == 255  a wall. Confirmed to only ever be 255 or 0
    //                               across every level, so it is a reliable
    //                               solid/open test.
    //   floorHeight                 shades open floor, so height changes read
    //                               as contour rather than flat space.
    //   scriptAction != 0           a cell that triggers something.
    //
    // Doors come from the level's own door records rather than the grid,
    // because a door's cells are only marked solid while it is shut and would
    // otherwise flicker between wall and floor as it opens.
    struct MinimapStyle
    {
        // Cell size in pixels. The whole map is scaled to fit the destination
        // rect, so this is a floor for legibility rather than an exact size.
        int minCellPixels = 2;

        bool drawPlayer = true;
        bool drawDoors = true;
        bool drawTriggers = false; // noisy on a live HUD, useful on the pause map
        bool drawBorder = true;

        // Letterboxing normally CENTRES the map in `dest`. The HUD wants it
        // pinned to the top-left instead, so the whole assembly starts exactly
        // where it is placed rather than floating toward the middle of a box
        // that is wider than the map needs (Edward, 2026).
        bool alignTopLeft = false;

        // 0-255. The live HUD version wants to be readable without hiding the
        // world behind it.
        Uint8 alpha = 255;
    };

    // Draws the map into `dest`, letterboxed to preserve the level's aspect
    // ratio. `playerGridX/Z` are grid coordinates in CELLS and `playerYaw` is
    // the camera yaw in radians; both are ignored when style.drawPlayer is
    // false.
    //
    // `visited` is one byte per cell, indexed the same way as the collision grid
    // (stride mapLength - see the note in the .cpp). Non-zero means the player
    // has been there and the cell is drawn; zero leaves it blank. Pass nullptr
    // to reveal the whole level, which is what owning an Auto Mapper does.
    //
    // WHOSE FOG THIS IS. The original's own visited tracking has NOT been
    // located: there is no map renderer in the decompilation to consume one, and
    // the only runtime flag written into a collision cell is bit 0x80 of byte
    // +0x07, which the door code sets - not visitation. So the behaviour here
    // (reveal the player's cell and its immediate neighbours) is this port's,
    // matching how the game plays rather than transcribed from it.
    // Returns the rect it actually drew into, in pixels. That is NOT `dest`:
    // the map is letterboxed to preserve the level's aspect, so a tall level in
    // a wide box fills only part of it. Callers that frame the map need the real
    // extent, or their border ends up far wider than the map (Edward, 2026).
    SDL_FRect DrawMinimap(SDL_Renderer* renderer,
                     const ALTEngine::Formats::LevelGeometry& level,
                     const SDL_FRect& dest,
                     float playerGridX, float playerGridZ, float playerYaw,
                     const MinimapStyle& style,
                     const std::vector<uint8_t>* visited = nullptr);

    // Marks the player's cell and its eight neighbours as seen. Sizes `visited`
    // to the level on first use, so the caller only has to keep the vector.
    //
    // Neighbours as well as the cell itself because revealing a single cell per
    // step leaves a one-cell trail that reads as a thin line rather than a
    // corridor - you cannot tell a passage from a room.
    void MarkMinimapVisited(const ALTEngine::Formats::LevelGeometry& level,
                            std::vector<uint8_t>& visited,
                            int playerCellX, int playerCellZ);
}
