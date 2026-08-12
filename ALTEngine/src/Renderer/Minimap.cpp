#include "Minimap.h"

#include <algorithm>
#include <cmath>

namespace ALTEngine::Renderer
{
    namespace
    {
        // The menu's palette, so the map does not look bolted on.
        // Cell roles, as they appear on the original's map (Edward, 2026):
        //   walls            dark green
        //   walkable space   light green
        //   doors            lime green
        //   crates/barrels   red
        // and nothing else - level triggers are NOT drawn, which they were here.
        //
        // THE EXACT VALUES ARE NOT TRACED. The one colour the decompilation gives
        // is FUN_00043cc4's modulate on the whole map quad, 0x4c,0x73,0x4c =
        // RGB(76,115,76). The per-cell colours live in the map TEXTURE, and the
        // code that builds that texture has not been located - the automap
        // functions around FUN_00043cc4 turned out to be its save/load text, not
        // the map raster.
        //
        // The builder (FUN_00043078) writes indices 0/1/2/4 per cell - nothing,
        // walkable, wall, and the Auto Mapper's reveal-all case - so the roles
        // below line up with the original's own set. Doors and obstacles are not
        // among those four, which means the original marks them from a later pass
        // rather than from the grid scan.
        //
        // The values are still a family built around the one known colour: the
        // modulate as the walkable tone, a darker multiple for walls, a
        // green-shifted brighter one for doors. Red has no relation to it. All four
        // should be replaced once the map texture's CLUT is located.
        constexpr SDL_Color FLOOR{ 76, 115, 76, 255 };     // the known modulate
        constexpr SDL_Color WALL{ 28, 44, 28, 255 };       // darker
        constexpr SDL_Color DOOR{ 140, 220, 60, 255 };     // lime
        constexpr SDL_Color OBSTACLE{ 180, 40, 40, 255 };  // crates, barrels
        constexpr SDL_Color PLAYER{ 255, 127, 0, 255 };    // ff,7f,00 from the code
        constexpr SDL_Color BACKDROP{ 0, 0, 0, 255 };
        constexpr SDL_Color BORDER{ 76, 115, 76, 255 };   // only when style.drawBorder

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
                bool blocking = ALTEngine::Formats::IsCellBlocking(level, x << 9, z << 9);
                put(x, z, blocking ? WALL : FLOOR);
            }
        }

        // Doors over the floor. Two cells along the door's own axis - see below.
        if (style.drawDoors)
        {
            for (const auto& door : level.doors)
            {
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

        // Crates and barrels - the obstacles the original marks in red.
        for (const auto& crate : level.crates)
        {
            int cx = static_cast<int>(crate.x);
            int cz = static_cast<int>(crate.y);
            size_t ci = static_cast<size_t>(cz) * gridW + cx;
            if (visited && (ci >= visited->size() || (*visited)[ci] == 0)) { continue; }
            put(cx, cz, OBSTACLE);
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
