#include "Minimap.h"

#include <algorithm>
#include <cmath>

namespace ALTEngine::Renderer
{
    namespace
    {
        // The menu's palette, so the map does not look bolted on.
        // Cell colours, SAMPLED FROM THE ORIGINAL'S OWN MAP.
        //
        // Decoding Edward's crop of the original pause map gives exactly FIVE
        // distinct colours - which is the five values FUN_00043078 writes per cell,
        // so the mapping is no longer guesswork:
        //
        //   rgb(  0, 24,  0)  x50656   value 0   backdrop: walls, void, unexplored
        //   rgb( 33, 65, 33)  x10964   value 1   walkable
        //   rgb( 16, 40, 16)  x 4720   value 2   a dimmer tier - see below
        //   rgb( 74, 40, 16)  x  176   value 5   VENT - a brown, not a green
        //   rgb( 16,113, 16)  x   48   value 4   the brightest green
        //
        // These replace the PANEL.PAL guesses. PANEL.PAL was the wrong source: none
        // of its entries is rgb(0,24,0) or rgb(74,40,16), so the map texture has its
        // own CLUT after all, exactly as the code implied.
        //
        // THERE IS NO RED ANYWHERE IN THE ORIGINAL MAP. So crates and barrels are
        // not drawn on it at all, and the red obstacles this had been drawing were
        // my invention - they are gone.
        //
        // At roughly 6.9 pixels per cell in that crop the counts work out to about
        // 1589 walkable cells, 684 dimmer ones, 25 vents and 7 bright - and 24 vents
        // is exactly what byte +10 == 6 finds on L111, which confirms the vent
        // colour.
        constexpr SDL_Color BACKDROP{ 0, 24, 0, 255 };
        constexpr SDL_Color FLOOR{ 33, 65, 33, 255 };
        constexpr SDL_Color OUTLINE{ 16, 40, 16, 255 };   // 1180 cells: the wall outline
        constexpr SDL_Color CRATE{ 74, 40, 16, 255 };     // 44 cells: L111 has exactly 44 crates
        constexpr SDL_Color DOOR{ 16, 113, 16, 255 };
        constexpr SDL_Color BORDER{ 33, 65, 33, 255 };
        constexpr SDL_Color PLAYER{ 255, 127, 0, 255 };   // ff,7f,00, from the code

        void SetColor(SDL_Renderer* renderer, const SDL_Color& c, Uint8 alpha)
        {
            Uint8 a = static_cast<Uint8>(c.a * alpha / 255);
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, a);
        }

        // Kept for reference; superseded by IsCellBlocking below.
        bool IsWallLegacy(const ALTEngine::Formats::CollisionNode& cell)
        {
            // Both bytes are confirmed to only ever be 255 or 0. Either one
            // marking solid is enough - they agree in practice, and treating a
            // disagreement as solid is the safe direction for a map.
            return cell.unknown3 == 255 || cell.unknown4 == 255;
        }
    }

    SDL_FRect DrawMinimap(SDL_Renderer* renderer,
                     const ALTEngine::Formats::LevelGeometry& level,
                     const SDL_FRect& dest,
                     float playerGridX, float playerGridZ, float playerYaw,
                     const MinimapStyle& style,
                     const std::vector<uint8_t>* visited)
    {
        // NOTE THE FIELD NAMES. Despite what they suggest, `mapLength` is the
        // X extent and the grid's row STRIDE, and `mapWidth` is the Z extent.
        // Taken from LevelLoader::IsCellBlocking, which indexes
        // `cellZ * mapLength + cellX` and bounds cellX by mapLength - and which
        // is ground truth because collision works in game.
        //
        // Using mapWidth as the stride instead drew the level as a field of
        // wall with one-cell-tall horizontal streaks through it, which is what
        // a stride error looks like.
        SDL_FRect drawn{ dest.x, dest.y, 0.0f, 0.0f };

        int gridW = static_cast<int>(level.header.mapLength);
        int gridH = static_cast<int>(level.header.mapWidth);
        if (gridW <= 0 || gridH <= 0) { return drawn; }
        if (level.collisionGrid.size() < static_cast<size_t>(gridW) * gridH) { return drawn; }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        // Letterbox: one scale for both axes so the level is not stretched.
        float scale = std::min(dest.w / static_cast<float>(gridW), dest.h / static_cast<float>(gridH));
        if (scale <= 0.0f) { return drawn; }
        // WHOLE PIXELS PER CELL. A fractional cell size means NEAREST sampling
        // gives some cells 4 pixels and others 5, which reads as an uneven stretch
        // (Edward, 2026). Flooring it makes every cell identical; the map just
        // occupies slightly less of its box.
        // Whole pixels per cell where there is room for it, since a fractional
        // cell makes NEAREST give some cells 4 pixels and others 5 - an uneven
        // stretch (Edward, 2026).
        //
        // Below one pixel per cell that is impossible: L111 is 92x105 cells and the
        // live HUD box is 196x68, so the scale is 0.65 and flooring it would blank
        // the map entirely. There the fractional scale is kept and the texture is
        // filtered down instead, which is the lesser evil - the pause map, where
        // detail actually matters, gets whole cells.
        float cell = scale;
        if (scale >= 1.0f)
        {
            cell = std::floor(scale);
        }
        float drawW = cell * gridW;
        float drawH = cell * gridH;
        float originX = style.alignTopLeft ? dest.x : dest.x + (dest.w - drawW) * 0.5f;
        float originY = style.alignTopLeft ? dest.y : dest.y + (dest.h - drawH) * 0.5f;

        SetColor(renderer, BACKDROP, style.alpha);
        SDL_FRect backdrop{ originX, originY, drawW, drawH };
        drawn = backdrop;
        SDL_RenderFillRect(renderer, &backdrop);


        // ONE PIXEL PER CELL, built into a texture and blitted - the structure
        // FUN_00043cc4 uses. Drawing a rect per cell could not avoid seams or
        // overlaps at a fractional scale however the edges were rounded; a
        // gridW x gridH image scaled up has neither by construction, and it is
        // also far less work per frame (Edward, 2026).
        static SDL_Texture* cached = nullptr;
        static int cachedW = 0, cachedH = 0;

        if (!cached || cachedW != gridW || cachedH != gridH)
        {
            if (cached) { SDL_DestroyTexture(cached); }
            cached = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC, gridW, gridH);
            if (!cached) { return drawn; }
            SDL_SetTextureBlendMode(cached, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(cached, SDL_SCALEMODE_NEAREST);
            cachedW = gridW;
            cachedH = gridH;
        }

        std::vector<uint8_t> pixels(static_cast<size_t>(gridW) * gridH * 4, 0);
        auto put = [&](int x, int z, const SDL_Color& c) {
            if (x < 0 || z < 0 || x >= gridW || z >= gridH) { return; }
            size_t i = (static_cast<size_t>(z) * gridW + x) * 4;
            pixels[i + 0] = c.r;
            pixels[i + 1] = c.g;
            pixels[i + 2] = c.b;
            pixels[i + 3] = c.a;
        };

        for (int z = 0; z < gridH; ++z)
        {
            for (int x = 0; x < gridW; ++x)
            {
                size_t index = static_cast<size_t>(z) * gridW + x;

                // Unseen cells stay fully transparent. nullptr means revealed.
                if (visited && (index >= visited->size() || (*visited)[index] == 0)) { continue; }

                const auto& c = level.collisionGrid[index];
                // The engine's OWN blocking test, not the two 255 bytes this used
                // to check.
                //
                // Those two only mark solid geometry, so anything blocking for
                // another reason - the dead space beside a door frame, an
                // unbreakable crate that is part of the level - came out as
                // walkable floor (Edward, 2026). IsCellBlocking is what the player
                // actually collides with, so it is the right question to ask, and
                // it is already proven correct because movement depends on it.
                //
                // It takes GAME coordinates, which are cells << 9.
                // CLASSIFICATION TRACED FROM FUN_00043078, which writes one of
                // five values per cell. Its logic, with the runtime cell as an
                // int* (16 bytes per cell):
                //
                //   if (DAT_00245bb4[cell[5]] == 0)            -> 0   not drawn
                //   else if (byte12 == 0 || byte12 < 0x14 || byte12 == 0xff) {
                //       if (cell[7] & 0x80)                    -> 2   wall
                //       else if (cell[10] == 6)                -> 5   VENT
                //       else if (cell[0] != 0 || (cell[7] & 1)) -> 1  walkable
                //       else                                   -> 0   nothing
                //   } else                                     -> 4   obstacle
                //
                // Which corrects two things I had guessed wrong: null space is a
                // LOOKUP on byte +5, not my neighbour heuristic, and there is a
                // fifth cell type at byte +10 == 6 - the vents facehuggers spawn
                // from, which the original draws as its own box rather than a bump
                // in the wall (Edward, 2026).
                //
                // Byte +7 is runtime-only and always 0 on disc, so its wall bit is
                // read here from unknown3/unknown4 (confirmed 255 = wall) and its
                // seen bit from our own visited tracking.
                const uint8_t* raw = reinterpret_cast<const uint8_t*>(&c);

                // WALLS ARE NOT DRAWN. This inverts what this function used to do,
                // and it is why the map never looked right.
                //
                // FUN_00043078's branch reads:
                //     if ((cell[7] & 0x80) == 0) { ... 5 / 1 / 0 ... } else { 2 }
                // so bit 0x80 selects value 2 - and FUN_00029704, the DOOR placement
                // code, is what sets that bit. Value 2 is therefore a DOOR cell, not
                // a wall. Nothing in the whole function emits a wall value at all:
                // solid geometry simply falls through to 0, "not drawn", exactly
                // like empty space.
                //
                // That matches the original's map, where the light lines are the
                // walkable corridors and everything else - walls and void alike - is
                // the flat dark backdrop.
                // NOT drawing byte+10 == 6 as its own colour. It finds 24 cells on
                // L111, but the original's only warm colour has 44 - which is the
                // crate count exactly. So that warm colour is crates, and whatever
                // byte +10 == 6 marks is not given its own colour on the map.
                (void)raw;

                // Blocking cells are left transparent, which reads as the backdrop.
                // IsCellBlocking stands in for the original's `cell[0] != 0` test,
                // since it is what the player actually collides with.
                // Walkable floor is drawn. A SOLID cell is drawn only if it touches
                // walkable floor, which gives the one-cell outline the original has
                // (1180 cells there, 1045 here). Every other solid cell is backdrop -
                // 5684 of them on L111, and colouring those was what flooded the map.
                if (!ALTEngine::Formats::IsCellBlocking(level, x << 9, z << 9))
                {
                    put(x, z, FLOOR);
                }
                else
                {
                    bool touchesFloor = false;
                    for (int dz = -1; dz <= 1 && !touchesFloor; ++dz)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            if (dx == 0 && dz == 0) { continue; }
                            int nx = x + dx, nz = z + dz;
                            if (nx < 0 || nz < 0 || nx >= gridW || nz >= gridH) { continue; }
                            if (!ALTEngine::Formats::IsCellBlocking(level, nx << 9, nz << 9))
                            {
                                touchesFloor = true;
                                break;
                            }
                        }
                    }
                    if (touchesFloor) { put(x, z, OUTLINE); }
                }
            }
        }

        // Doors over the floor. Two cells along the door's own axis - see below.
        if (style.drawDoors)
        {
            for (const auto& door : level.doors)
            {
                // FUN_00029704 writes bit 0x80 on only the FIRST and LAST cell of
                // the door's four-cell span (iVar15 + 7 and iVar15 + 0x37), not the
                // two in between - which is exactly why the original's doors read as
                // 2 cells wide on the map (Edward, 2026).
                bool alongZ = (door.rotation == 2 || door.rotation == 6);
                for (int i = 0; i < 2; ++i)
                {
                    int dx = static_cast<int>(door.x) + (alongZ ? 0 : i);
                    int dz = static_cast<int>(door.y) + (alongZ ? i : 0);
                    size_t di = static_cast<size_t>(dz) * gridW + dx;
                    if (visited && (di >= visited->size() || (*visited)[di] == 0)) { continue; }
                    put(dx, dz, DOOR);
                }
            }
        }

        // Crates, in red. L111 has 44 and the original's map has exactly 44 cells of
        // #4A2810, so this is confirmed by count rather than assumed. They were right
        // the first time; I removed them on the false grounds that the map had no red.
        for (const auto& crate : level.crates)
        {
            int cx = static_cast<int>(crate.x);
            int cz = static_cast<int>(crate.y);
            size_t ci = static_cast<size_t>(cz) * gridW + cx;
            if (visited && (ci >= visited->size() || (*visited)[ci] == 0)) { continue; }
            put(cx, cz, CRATE);
        }

        SDL_UpdateTexture(cached, nullptr, pixels.data(), gridW * 4);
        SDL_SetTextureAlphaMod(cached, style.alpha);

        SDL_FRect blit{ originX, originY, drawW, drawH };
        SDL_RenderTexture(renderer, cached, nullptr, &blit);
        SDL_SetTextureAlphaMod(cached, 255);

        if (style.drawPlayer)
        {
            float px = originX + playerGridX * cell;
            float pz = originY + playerGridZ * cell;

            // A short line for facing plus a dot for position - readable at any
            // map scale, unlike a triangle that vanishes on a big level.
            float length = std::max(cell * 3.0f, 6.0f);
            float fx = px + std::sin(playerYaw) * length;
            float fz = pz - std::cos(playerYaw) * length;

            SetColor(renderer, PLAYER, style.alpha);
            SDL_RenderLine(renderer, px, pz, fx, fz);

            float dot = std::max(cell, 2.0f);
            SDL_FRect marker{ px - dot * 0.5f, pz - dot * 0.5f, dot, dot };
            SDL_RenderFillRect(renderer, &marker);
        }

        if (style.drawBorder)
        {
            SetColor(renderer, BORDER, style.alpha);
            SDL_RenderRect(renderer, &backdrop);
        }

        return drawn;
    }
}

namespace ALTEngine::Renderer
{
    void MarkMinimapVisited(const ALTEngine::Formats::LevelGeometry& level,
                            std::vector<uint8_t>& visited,
                            int playerCellX, int playerCellZ)
    {
        int gridW = static_cast<int>(level.header.mapLength);
        int gridH = static_cast<int>(level.header.mapWidth);
        if (gridW <= 0 || gridH <= 0) { return; }

        size_t needed = static_cast<size_t>(gridW) * gridH;
        if (visited.size() != needed) { visited.assign(needed, 0); }

        for (int dz = -1; dz <= 1; ++dz)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                int x = playerCellX + dx;
                int z = playerCellZ + dz;
                if (x < 0 || z < 0 || x >= gridW || z >= gridH) { continue; }
                visited[static_cast<size_t>(z) * gridW + x] = 1;
            }
        }
    }
}
