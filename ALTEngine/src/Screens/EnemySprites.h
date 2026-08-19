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
    // WHERE THEIR ARTWORK LIVES: cd\NME\<NAME>.B16, one file per creature, with a
    // .BND alongside. Fifteen of them, listed in a 12-byte-per-entry table at
    // 0x000a5e10.
    //
    // MONSTER TYPE -> CREATURE, from Edward's ALTViewer notes:
    //
    //     0x01 Egg                 EGGS        0x09 Ceiling Dog Alien  DOGCEIL
    //     0x02 Face Hugger         HUGGER      0x0a Colonist           COLONIST
    //     0x03 Chest Burster       BURSTER     0x0b Guard              GUARD
    //     0x04 Bambi               BAMBI       0x0c Soldier            SOLDIER
    //     0x05 Dog Alien           DOG         0x0d Synthetic          SYNTH
    //     0x06 Warrior Drone       WAR         0x0e Handler            HANDLER
    //     0x07 Queen               QUEEN       0x0f unused in every level
    //     0x08 Ceiling Warrior     WARCEIL
    //
    // THIS WAS WRONG, and the way it was wrong is worth recording. I had been
    // indexing the FILE TABLE by type - 1. That table's order is its own
    // (DOG, HUGGER, WAR, EGGS, HANDLER, SYNTH, ...) and is not the type order -
    // but type 2 lands on HUGGER under both readings, and that single coincidence
    // made the whole scheme look confirmed. Level 1-1's other creature is type 6,
    // the Warrior Drone, and it was loading SYNTH instead (Edward, 2026: "the
    // aliens in the first level have the wrong sprite").
    //
    // The lesson is the same one as the barrel/crate effect tables: one matching
    // data point is not a confirmed mapping, and I should have said so at the
    // time rather than calling it likely.
    //
    // FINGERS.B16 has no monster type because it is NOT a creature in the world:
    // it is the overlay of a face hugger crawling onto the camera (Edward, 2026).
    // The pounce draws it - see EnemyBehaviour's note on FUN_000303ec, which
    // claims two sprite slots and reads its frames from the creature's animation
    // table at +0x88 and +0x8c.
    //
    // TYPES 0x10-0x13 ARE THE VENTS - horizontal and vertical steam and flame.
    // They occupy monster slots rather than being separate scenery, which places
    // the "Steam valve closed" and "Flame jet shut down" notices (string indices
    // 84 and 85) in the monster system rather than a hazard one of their own.
    inline constexpr const char* ENEMY_FILE_FOR_TYPE[] = {
        nullptr,      // 0x00
        "EGGS",       // 0x01
        "HUGGER",     // 0x02
        "BURSTER",    // 0x03
        "BAMBI",      // 0x04
        "DOG",        // 0x05
        "WAR",        // 0x06
        "QUEEN",      // 0x07
        "WARCEIL",    // 0x08
        "DOGCEIL",    // 0x09
        "COLONIST",   // 0x0a
        "GUARD",      // 0x0b
        "SOLDIER",    // 0x0c
        "SYNTH",      // 0x0d
        "HANDLER",    // 0x0e
        nullptr,      // 0x0f
    };
    inline constexpr int ENEMY_TYPE_COUNT = 16;

    inline constexpr int VENT_STEAM_HORIZONTAL = 0x10;
    inline constexpr int VENT_FLAME_HORIZONTAL = 0x11;
    inline constexpr int VENT_STEAM_VERTICAL = 0x12;
    inline constexpr int VENT_FLAME_VERTICAL = 0x13;

    inline bool IsVent(int monsterType)
    {
        return monsterType >= VENT_STEAM_HORIZONTAL && monsterType <= VENT_FLAME_VERTICAL;
    }

    inline const char* EnemyFileForType(int monsterType)
    {
        if (monsterType < 0 || monsterType >= ENEMY_TYPE_COUNT) { return nullptr; }
        return ENEMY_FILE_FOR_TYPE[monsterType];
    }

    // FRAMES: WHAT IS ESTABLISHED, AND WHAT IS NOT.
    //
    // TRACED. FUN_0002f4b0 starts an animation on an entity:
    //     pair = entity[+0x7c] + animIndex * 8
    //     entity[+0x94] = pair[0]     the frame table
    //     entity[+0x98] = pair[1] + 1 the sequence program counter
    //     entity[+0xb2] = pair[1][0]  the frame duration
    //     entity[+0xc4] = animIndex
    // then calls FUN_00028a6c - THE SAME ANIMATOR THE WEAPONS USE. It operates on
    // entity+0x80 as its struct base, which is why every field above is exactly
    // 0x80 above the weapon animator's equivalent.
    //
    // And the animator resolves a frame as:
    //     entity[+0x90] = entity[+0x94] + sequenceFrameIndex * 0xc
    // so 12 bytes per record, indexed straight by the sequence's frame number.
    // That confirms the record layout and that enemies and weapons share one
    // animation system.
    //
    // NOT ESTABLISHED: where the eight view directions come from.
    //
    // They are not in the animator - it applies no view offset at all - and they
    // are not spare capacity in the frame tables, which are sized exactly to the
    // frames their sequence indexes (the first creature's animation 0 has an
    // 8-entry table for an 8-frame sequence, with nothing left over). So the view
    // must be applied by the draw call, and I have not found it.
    //
    // Because of that, THE VIEW CODE BELOW IS NOT USED FOR CHOOSING ARTWORK yet.
    // It computed a view index and loaded frames 0-4 of section 0 as if they were
    // the five stored views, which was simply wrong - sections are poses and the
    // frames inside them are animation, as the EGGS override dump shows (six
    // sections of 3, 3, 5, 6, 8 and 8 frames). That mistake is what put the wrong
    // starting frame on screen.
    //
    // Until the view mechanism is found, one frame is loaded per creature - the
    // first frame of section 0 - so the right creature appears in its resting
    // pose rather than an arbitrary frame of an arbitrary pose. The view helpers
    // are kept because the eight-view, five-frame mirroring scheme is recorded in
    // the port's own earlier notes and will be needed once the draw is traced.
    //
    // THE ONE THING THAT WOULD SETTLE IT: the F0## section and frame inventory of
    // HUGGER.B16. If a section holds eight times its animation length, the views
    // live inside the section and the frame index is a stride. If it holds exactly
    // the animation length, the views are separate sections and the animation
    // index carries them.

    class EnemySprites
    {
    public:
        static constexpr int VIEW_COUNT = 8;
        static constexpr int UNIQUE_VIEWS = 5;   // the rest are mirrored
        static constexpr int REST_SECTION = 0;   // GUESS - which pose section is idle

        // One frame until the view mechanism is traced - see the note above.
        static constexpr int FRAMES_TO_LOAD = 1;

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
                for (int frameIndex = 0; frameIndex < FRAMES_TO_LOAD; ++frameIndex)
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

                // NOT DRAWN IN THE WORLD WHILE POUNCING. The original replaces the
                // creature's handler with LAB_00030918 for the duration, so
                // FUN_000300a4 - the draw dispatcher - is no longer reached for it
                // at all. The creature is on your face, not on the floor, and
                // leaving it standing there was plainly wrong (Edward, 2026).
                if (enemy.pouncing) { continue; }

                auto it = byType.find(enemy.type);
                if (it == byType.end()) { continue; }

                const float dx = cameraX - enemy.x;
                const float dz = cameraZ - enemy.z;
                // The view is computed but NOT used to pick artwork - see the
                // note above. Only frame 0 is loaded, so that is what draws.
                (void)dx; (void)dz;
                const int view = 0;
                if (!it->second.present[0]) { continue; }

                ALTEngine::Renderer::PlacedSprite sprite;
                sprite.textureKey = SheetKey(enemy.type, view);
                sprite.x = enemy.x;
                sprite.z = enemy.z;
                sprite.halfWidth = it->second.width[view] * 0.5f * WORLD_UNITS_PER_PIXEL;
                sprite.halfHeight = it->second.height[view] * 0.5f * WORLD_UNITS_PER_PIXEL;

                // Standing on the floor, not centred on it.
                sprite.y = enemy.y + sprite.halfHeight;


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
