#pragma once

#include "../Formats/SpriteFrameLoader.h"
#include "../Renderer/ModelRenderer.h"
#include <SDL3/SDL.h>

#include "Enemies.h"
#include "PlayerCamera.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace ALTEngine::Screens
{
    // Drawing enemies.
    //
    // WHERE THEIR ARTWORK LIVES: cd\NME\<NAME>.B16, one file per creature.
    //
    // My first pass concluded there was no enemy sprite file, because a scan of
    // the image for filename-shaped strings found only the weapons and the level
    // GFX sets. That scan was simply incomplete - the byte capture misses part
    // of the string region, and the paths are stored with a directory prefix
    // ("cd\NME\HUGGER.B16") which the pattern I used did not match. Fifteen of
    // them are there, each with a .BND alongside.
    //
    // The index is a 12-byte-per-entry table at 0x000a5e10, filename pointer
    // first:
    //     0 DOG        1 HUGGER    2 WAR       3 EGGS      4 HANDLER
    //     5 SYNTH      6 GUARD     7 SOLDIER   8 BURSTER   9 BAMBI
    //    10 FINGERS   11 WARCEIL  12 QUEEN    13 DOGCEIL  14 COLONIST
    //
    // MARKED: whether the monster record's type byte indexes this table
    // directly is NOT traced. It probably does not - L111's roster is types 2
    // and 6, which would be WAR and GUARD, but the type-2 monsters are the ones
    // crates spring at the player and the game's own sound bank gives level 1
    // the SYNDIE/SYNFALL/SYNHIT set. HUGGER and SYNTH are the creatures that
    // fit, and those are indices 1 and 5 - one less than the record's type in
    // both cases. So a one-based type is the likely reading and is what is used
    // here, but it rests on two data points and should be checked.
    inline constexpr const char* ENEMY_FILES[] = {
        "DOG", "HUGGER", "WAR", "EGGS", "HANDLER", "SYNTH", "GUARD", "SOLDIER",
        "BURSTER", "BAMBI", "FINGERS", "WARCEIL", "QUEEN", "DOGCEIL", "COLONIST",
    };
    inline constexpr int ENEMY_FILE_COUNT = 15;

    // GUESS - see above. Type 2 gives HUGGER, type 6 gives SYNTH.
    inline const char* EnemyFileForType(int monsterType)
    {
        const int index = monsterType - 1;
        if (index < 0 || index >= ENEMY_FILE_COUNT) { return nullptr; }
        return ENEMY_FILES[index];
    }

    // SECTIONS ARE POSES, NOT VIEWS. The override dump for EGGS shows six
    // sections with 3, 3, 5, 6, 8 and 8 frames - so an F0## section is an
    // animation (idle, attack, death and so on) and the frames within it are
    // that animation's frames. An earlier version of this file assumed a section
    // held the five view angles, which is wrong.
    //
    // Which section is which pose is not traced. Section 0 is used here as the
    // resting pose.
    //
    // EIGHT VIEWS, FIVE FRAMES. The frame shown depends on the angle between
    // where the enemy faces and where it is being looked at from, quantised to
    // eight. Views 5, 6 and 7 are views 3, 2 and 1 mirrored - the artwork only
    // holds front, front-quarter, side, back-quarter and back, and the engine
    // flips the other three. That is the standard scheme for this era and the
    // port's own notes already record it for these sprites.
    //
    // MARKED: the section index per enemy type, and which section holds which
    // pose, are NOT traced. Section == type is the assumption, and it is the one
    // to check first if the wrong creature appears.
    class EnemySprites
    {
    public:
        static constexpr int VIEW_COUNT = 8;
        static constexpr int UNIQUE_VIEWS = 5;   // the rest are mirrored
        static constexpr int REST_SECTION = 0;   // GUESS - which pose section is idle

        // Which stored view a given eighth maps to, and whether it is flipped.
        // 0..4 are held as artwork; 5, 6 and 7 are 3, 2 and 1 mirrored.
        static int ViewFrame(int eighth) { return (eighth <= 4) ? eighth : (VIEW_COUNT - eighth); }
        static bool ViewMirrored(int eighth) { return eighth > 4; }

        // The eighth to show, from the enemy's facing and the direction the
        // camera sees it from.
        static int ViewIndex(int enemyFacing, float dx, float dz)
        {
            // Angle from the enemy to the viewer, in the same 4096 units.
            const float radians = std::atan2(dx, -dz);
            int toViewer = static_cast<int>(std::lround(
                radians * (PlayerCamera::ANGLE_UNITS / (2.0f * 3.14159265358979323846f))));
            int relative = (toViewer - enemyFacing) & PlayerCamera::ANGLE_MASK;

            // Round to the nearest eighth rather than truncating, so the
            // changeover happens halfway between views.
            const int eighth = PlayerCamera::ANGLE_UNITS / VIEW_COUNT;
            return ((relative + eighth / 2) / eighth) & (VIEW_COUNT - 1);
        }

        // Loads whatever sections the level's graphics file holds for the enemy
        // types in play. Missing frames are not an error - a level only ships
        // the creatures it uses.
        bool Load(const std::filesystem::path& cdDirectory, const std::string& /*levelDigits*/,
                  const std::vector<Enemies::Enemy>& enemies)
        {
            std::vector<int> wanted;
            for (const auto& enemy : enemies)
            {
                if (std::find(wanted.begin(), wanted.end(), enemy.type) == wanted.end())
                {
                    wanted.push_back(enemy.type);
                }
            }

            int loadedTypes = 0;
            for (int type : wanted)
            {
                const char* name = EnemyFileForType(type);
                if (!name) { SDL_Log("EnemySprites: monster type %d has no file", type); continue; }

                std::filesystem::path path = cdDirectory / "NME" / (std::string(name) + ".B16");
                std::error_code ec;
                if (!std::filesystem::exists(path, ec))
                {
                    SDL_Log("EnemySprites: %s not found for monster type %d",
                            path.string().c_str(), type);
                    continue;
                }

                TypeFrames frames;
                for (int frameIndex = 0; frameIndex < UNIQUE_VIEWS; ++frameIndex)
                {
                    std::optional<ALTEngine::Formats::SpriteFrameInfo> frame;
                    try
                    {
                        frame = ALTEngine::Formats::SpriteFrameLoader::LoadFrame(
                            path, name, REST_SECTION, frameIndex);
                    }
                    catch (const std::exception&) { break; }

                    if (!frame || frame->width <= 0 || frame->height <= 0 || frame->rgba.empty()) { break; }

                    const std::string key = SheetKey(type, frameIndex);
                    if (!ALTEngine::Renderer::ModelRenderer::UploadSpriteSheet(
                            key, frame->rgba, frame->width, frame->height))
                    {
                        break;
                    }
                    frames.width[frameIndex] = frame->width;
                    frames.height[frameIndex] = frame->height;
                    frames.present[frameIndex] = true;
                    frames.any = true;
                }
                if (frames.any)
                {
                    byType[type] = frames;
                    loadedTypes++;
                    SDL_Log("EnemySprites: monster type %d -> %s.B16", type, name);
                }
            }
            return loadedTypes > 0;
        }

        bool Ready() const { return !byType.empty(); }

        void Collect(const std::vector<Enemies::Enemy>& enemies,
                     float cameraX, float cameraZ,
                     std::vector<ALTEngine::Renderer::PlacedSprite>& out) const
        {
            for (const auto& enemy : enemies)
            {
                if (!enemy.active || !enemy.alive) { continue; }

                auto it = byType.find(enemy.type);
                if (it == byType.end()) { continue; }

                const float dx = cameraX - enemy.x;
                const float dz = cameraZ - enemy.z;
                const int eighth = ViewIndex(enemy.facing, dx, dz);
                int view = ViewFrame(eighth);
                if (!it->second.present[view])
                {
                    // Fall back to whatever the type does have rather than
                    // dropping the enemy entirely.
                    view = 0;
                    if (!it->second.present[0]) { continue; }
                }

                ALTEngine::Renderer::PlacedSprite sprite;
                sprite.textureKey = SheetKey(enemy.type, view);
                sprite.x = enemy.x;
                sprite.z = enemy.z;
                sprite.halfWidth = it->second.width[view] * 0.5f * WORLD_UNITS_PER_PIXEL;
                sprite.halfHeight = it->second.height[view] * 0.5f * WORLD_UNITS_PER_PIXEL;

                // Standing on the floor, not centred on it.
                sprite.y = enemy.y + sprite.halfHeight;

                // Mirrored views are the same artwork with the UVs swapped.
                if (ViewMirrored(eighth)) { sprite.u0 = 1.0f; sprite.u1 = 0.0f; }
                out.push_back(sprite);
            }
        }

    private:
        struct TypeFrames
        {
            int width[UNIQUE_VIEWS] = { 0, 0, 0, 0, 0 };
            int height[UNIQUE_VIEWS] = { 0, 0, 0, 0, 0 };
            bool present[UNIQUE_VIEWS] = { false, false, false, false, false };
            bool any = false;
        };

        // Sprite pixels to world units. GUESS - the original scales its sprites
        // by distance in screen space, so there is no world size to read. Tuned
        // so a facehugger-sized frame is roughly ankle height in a 1536-unit
        // room.
        static constexpr float WORLD_UNITS_PER_PIXEL = 8.0f;

        static std::string SheetKey(int type, int view)
        {
            return "enemy:" + std::to_string(type) + ":" + std::to_string(view);
        }

        std::map<int, TypeFrames> byType;
    };
}
