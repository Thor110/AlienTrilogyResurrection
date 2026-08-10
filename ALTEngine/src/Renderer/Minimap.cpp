#include "Minimap.h"

#include <algorithm>
#include <cmath>

namespace ALTEngine::Renderer
{
    namespace
    {
        // The menu's palette, so the map does not look bolted on.
        constexpr SDL_Color WALL{ 0, 96, 0, 255 };
        constexpr SDL_Color FLOOR_LOW{ 0, 24, 0, 255 };
        constexpr SDL_Color FLOOR_HIGH{ 0, 56, 0, 255 };
        constexpr SDL_Color DOOR{ 200, 170, 0, 255 };
        constexpr SDL_Color TRIGGER{ 0, 110, 110, 255 };
        constexpr SDL_Color PLAYER{ 255, 255, 255, 255 };
        constexpr SDL_Color BORDER{ 0, 140, 0, 255 };
        constexpr SDL_Color BACKDROP{ 0, 0, 0, 190 };

        void SetColor(SDL_Renderer* renderer, const SDL_Color& c, Uint8 alpha)
        {
            Uint8 a = static_cast<Uint8>(c.a * alpha / 255);
            SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, a);
        }

        bool IsWall(const ALTEngine::Formats::CollisionNode& cell)
        {
            // Both bytes are confirmed to only ever be 255 or 0. Either one
            // marking solid is enough - they agree in practice, and treating a
            // disagreement as solid is the safe direction for a map.
            return cell.unknown3 == 255 || cell.unknown4 == 255;
        }
    }

    void DrawMinimap(SDL_Renderer* renderer,
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
        int gridW = static_cast<int>(level.header.mapLength);
        int gridH = static_cast<int>(level.header.mapWidth);
        if (gridW <= 0 || gridH <= 0) { return; }
        if (level.collisionGrid.size() < static_cast<size_t>(gridW) * gridH) { return; }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        // Letterbox: one scale for both axes so the level is not stretched.
        float scale = std::min(dest.w / static_cast<float>(gridW), dest.h / static_cast<float>(gridH));
        if (scale <= 0.0f) { return; }
        float cell = std::max(scale, static_cast<float>(style.minCellPixels) * 0.0f + scale);
        float drawW = cell * gridW;
        float drawH = cell * gridH;
        float originX = dest.x + (dest.w - drawW) * 0.5f;
        float originY = dest.y + (dest.h - drawH) * 0.5f;

        SetColor(renderer, BACKDROP, style.alpha);
        SDL_FRect backdrop{ originX, originY, drawW, drawH };
        SDL_RenderFillRect(renderer, &backdrop);

        // Floor height range, so the shading uses the level's own spread rather
        // than an arbitrary fixed scale. A flat level then reads as one tone
        // instead of all-black.
        int minFloor = 255, maxFloor = 0;
        for (int z = 0; z < gridH; ++z)
        {
            for (int x = 0; x < gridW; ++x)
            {
                const auto& c = level.collisionGrid[static_cast<size_t>(z) * gridW + x];
                if (IsWall(c)) { continue; }
                minFloor = std::min(minFloor, static_cast<int>(c.floorHeight));
                maxFloor = std::max(maxFloor, static_cast<int>(c.floorHeight));
            }
        }
        int floorRange = std::max(1, maxFloor - minFloor);

        for (int z = 0; z < gridH; ++z)
        {
            for (int x = 0; x < gridW; ++x)
            {
                size_t index = static_cast<size_t>(z) * gridW + x;

                // Unseen cells are left blank. nullptr means fully revealed,
                // i.e. the player has an Auto Mapper.
                if (visited && (index >= visited->size() || (*visited)[index] == 0)) { continue; }

                const auto& c = level.collisionGrid[index];
                SDL_FRect r{ originX + x * cell, originY + z * cell, cell + 0.5f, cell + 0.5f };

                if (IsWall(c))
                {
                    SetColor(renderer, WALL, style.alpha);
                }
                else if (style.drawTriggers && c.scriptAction != 0)
                {
                    SetColor(renderer, TRIGGER, style.alpha);
                }
                else
                {
                    float t = static_cast<float>(c.floorHeight - minFloor) / static_cast<float>(floorRange);
                    SDL_Color shade{
                        static_cast<Uint8>(FLOOR_LOW.r + (FLOOR_HIGH.r - FLOOR_LOW.r) * t),
                        static_cast<Uint8>(FLOOR_LOW.g + (FLOOR_HIGH.g - FLOOR_LOW.g) * t),
                        static_cast<Uint8>(FLOOR_LOW.b + (FLOOR_HIGH.b - FLOOR_LOW.b) * t),
                        255
                    };
                    SetColor(renderer, shade, style.alpha);
                }
                SDL_RenderFillRect(renderer, &r);
            }
        }

        // Doors from their own records - see the header for why not the grid.
        if (style.drawDoors)
        {
            SetColor(renderer, DOOR, style.alpha);
            for (const auto& door : level.doors)
            {
                // A door spans 4 cells along its own axis. rotation 2 and 6 run
                // along Z, the same test the door mesh placement uses.
                bool alongZ = (door.rotation == 2 || door.rotation == 6);
                for (int i = 0; i < 4; ++i)
                {
                    int dx = static_cast<int>(door.x) + (alongZ ? 0 : i);
                    int dz = static_cast<int>(door.y) + (alongZ ? i : 0);
                    if (dx < 0 || dz < 0 || dx >= gridW || dz >= gridH) { continue; }
                    size_t di = static_cast<size_t>(dz) * gridW + dx;
                    if (visited && (di >= visited->size() || (*visited)[di] == 0)) { continue; }
                    SDL_FRect r{ originX + dx * cell, originY + dz * cell, cell + 0.5f, cell + 0.5f };
                    SDL_RenderFillRect(renderer, &r);
                }
            }
        }

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
