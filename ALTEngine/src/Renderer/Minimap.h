#pragma once

#include "../Formats/LevelLoader.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

namespace ALTEngine::Renderer
{
    // Top-down map of a level, drawn from the collision grid.
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
    void DrawMinimap(SDL_Renderer* renderer,
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
