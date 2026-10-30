#pragma once

#include "../Formats/SpriteFrameLoader.h"
#include "../Renderer/ModelRenderer.h"
#include <SDL3/SDL.h>

#include "Enemies.h"
#include "PlayerCamera.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include "../Formats/EnemyAnimData.h"
#include <map>
#include <string>
#include <utility>
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

    // The aliens: the egg, hugger, burster, bambi, dog, warrior, queen and the
    // two ceiling creatures. Not the colonist (a person, cocooned), the guard,
    // soldier, synth and handler, or the vents.
    inline constexpr bool IsXenomorph(int monsterType)
    {
        return monsterType >= 0x01 && monsterType <= 0x09;
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
            namespace AD = ALTEngine::Formats::EnemyAnimData;

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
                const AD::Creature* creature = AD::ForType(type);
                if (!creature) { continue; }

                std::filesystem::path path = cdDirectory / "NME" / (std::string(creature->file) + ".B16");
                std::error_code ec;
                if (!std::filesystem::exists(path, ec)) { continue; }

                auto set = std::make_shared<ALTEngine::Formats::EnemySpriteSet>();
                if (!set->Load(path)) { continue; }

                // Decode every animation the behaviour switches can ask for, so a
                // creature never stalls waiting for artwork mid-fight.
                TypeArt art;
                art.sprites = set;
                int decodedAnims = 0;
                std::string missing;   // animations no section of the file matches

                for (int a = 0; a < creature->count; ++a)
                {
                    const AD::Anim& anim = creature->anims[a];
                    if (anim.count <= 0) { continue; }

                    std::vector<ALTEngine::Formats::EnemySpriteSet::Frame> records;
                    for (int i = 0; i < anim.count; ++i)
                    {
                        ALTEngine::Formats::EnemySpriteSet::Frame f;
                        f.offsetX = anim.frames[i].ox;
                        f.offsetY = anim.frames[i].oy;
                        f.width = anim.frames[i].w;
                        f.height = anim.frames[i].h;
                        f.compressedOffset = anim.frames[i].off;
                        records.push_back(f);
                    }

                    auto decoded = set->DecodeAnimation(records);
                    if (decoded.empty())
                    {
                        missing += (missing.empty() ? "" : " ") + std::to_string(a);
                        continue;
                    }

                    // Upload each frame as its own sheet.
                    Animation entry;
                    for (size_t i = 0; i < decoded.size(); ++i)
                    {
                        const std::string key = SheetKey(type, a, static_cast<int>(i));
                        if (!ALTEngine::Renderer::ModelRenderer::UploadSpriteSheet(
                                key, ToRgba(*set, decoded[i]), decoded[i].width, decoded[i].height))
                        {
                            continue;
                        }
                        entry.frames.push_back(Frame{ key, decoded[i].width, decoded[i].height,
                                                      decoded[i].offsetX, decoded[i].offsetY });
                    }
                    if (!entry.frames.empty())
                    {
                        art.animations[a] = std::move(entry);
                        decodedAnims++;
                    }
                }

                if (decodedAnims > 0)
                {
                    byType[type] = std::move(art);
                    loadedTypes++;
                    SDL_Log("EnemySprites: type %d -> %s.B16, %d of %d animations decoded%s%s",
                            type, creature->file, decodedAnims, creature->count,
                            missing.empty() ? "" : ", not: ", missing.c_str());
                }
            }
            return loadedTypes > 0;
        }

        bool Ready() const { return !byType.empty(); }

        // How many frames an animation has, 0 if this level's sprites lack it.
        int FrameCount(int type, int animation) const
        {
            auto it = byType.find(type);
            if (it == byType.end()) { return 0; }
            auto anim = it->second.animations.find(animation);
            return anim == it->second.animations.end() ? 0 : static_cast<int>(anim->second.frames.size());
        }

        void Collect(const std::vector<Enemies::Enemy>& enemies,
                     std::vector<ALTEngine::Renderer::PlacedSprite>& out) const
        {
            for (const auto& enemy : enemies)
            {
                if (!Enemies::BodyShown(enemy)) { continue; }

                // NOT DRAWN IN THE WORLD WHILE POUNCING - the original replaces
                // the creature's handler, so its draw is never reached. It is on
                // your face, not on the floor.
                if (enemy.pouncing) { continue; }

                auto it = byType.find(enemy.type);
                if (it == byType.end()) { continue; }

                auto anim = it->second.animations.find(enemy.animation);
                if (anim == it->second.animations.end() || anim->second.frames.empty())
                {
                    // A creature whose sprite set lacks this animation (a leap,
                    // say) still shows, in its ordinary one, rather than
                    // vanishing while it can still be shot.
                    anim = it->second.animations.find(EnemyBehaviour::ActingAnimation(enemy.type));
                }
                if (anim == it->second.animations.end() || anim->second.frames.empty())
                {
                    // Nor that: the first animation the set has, so a creature
                    // whose task picks animations by view (the colonist) is
                    // still drawn.
                    for (anim = it->second.animations.begin(); anim != it->second.animations.end(); ++anim)
                    {
                        if (!anim->second.frames.empty()) { break; }
                    }
                }
                if (anim == it->second.animations.end() || anim->second.frames.empty())
                {
                    continue;
                }

                // Wrap, so an animation that outlives its frame count loops rather
                // than falling off the end.
                const auto& frames = anim->second.frames;
                const size_t index = (enemy.holdAnimation || Enemies::Dead(enemy))
                    ? std::min(static_cast<size_t>(enemy.animationFrame), frames.size() - 1)
                    : static_cast<size_t>(enemy.animationFrame) % frames.size();
                const Frame& frame = frames[index];

                ALTEngine::Renderer::PlacedSprite sprite;
                sprite.textureKey = frame.key;
                // Walk views 5-7 are 3-1 flipped (FUN_000314b8, +0xb6 bit 5).
                if (enemy.mirrored) { std::swap(sprite.u0, sprite.u1); }
                sprite.x = enemy.x;
                sprite.z = enemy.z;
                sprite.halfWidth = frame.width * 0.5f * WORLD_UNITS_PER_PIXEL;
                sprite.halfHeight = frame.height * 0.5f * WORLD_UNITS_PER_PIXEL;

                // The frame hangs from the creature's anchor by its own offset,
                // as the original draws it: offsetY is where the frame's top sits
                // above the anchor (negative = up), so its bottom is
                // offsetY + height below it. The anchor is the creature's
                // position, 0x20 over the floor (FUN_00031f3c). Drawing the
                // frame's bottom at the anchor, as this did, left every body a
                // good way above the floor (Edward, 2026).
                //
                // The original paints sprites over the floor; here the depth
                // test would hide what goes below it, so on the ground a frame's
                // bottom stops at the floor. In the air (a leap, a fall) it keeps
                // its offset.
                float bottom = enemy.y - static_cast<float>(frame.offsetY + frame.height) * WORLD_UNITS_PER_PIXEL;
                if (enemy.vy == 0.0f) { bottom = std::max(bottom, enemy.y - EnemyBehaviour::STAND_CLEARANCE); }
                sprite.y = bottom + sprite.halfHeight;
                // Hanging from the ceiling: the frame's top at the ceiling.
                if (enemy.onCeiling) { sprite.y = enemy.y + EnemyBehaviour::STAND_CLEARANCE - sprite.halfHeight; }

                out.push_back(sprite);
            }
        }

    private:
        struct Frame
        {
            std::string key;
            int width = 0;
            int height = 0;
            int offsetX = 0;
            int offsetY = 0;
        };

        struct Animation { std::vector<Frame> frames; };

        struct TypeArt
        {
            std::shared_ptr<ALTEngine::Formats::EnemySpriteSet> sprites;
            std::map<int, Animation> animations;
        };

        // Sprite pixels to world units. GUESS - the original scales by distance in
        // screen space, so there is no world size to read.
        static constexpr float WORLD_UNITS_PER_PIXEL = 8.0f;

        static std::string SheetKey(int type, int animation, int frame)
        {
            return "enemy:" + std::to_string(type) + ":" + std::to_string(animation)
                 + ":" + std::to_string(frame);
        }

        // Palette indices to RGBA, using the creature file's own C000 palette.
        // Index 0 is transparent, as everywhere else in these files.
        static std::vector<uint8_t> ToRgba(const ALTEngine::Formats::EnemySpriteSet& set,
                                           const ALTEngine::Formats::EnemySpriteSet::Frame& f)
        {
            std::vector<uint8_t> rgba(static_cast<size_t>(f.width) * f.height * 4, 0);
            const auto& pal = set.Palette();
            for (size_t i = 0; i < f.pixels.size() && i * 4 + 3 < rgba.size(); ++i)
            {
                const uint8_t index = f.pixels[i];
                if (index == 0) { continue; }
                const size_t p = static_cast<size_t>(index) * 2;
                if (p + 1 >= pal.size()) { continue; }
                const uint16_t entry = static_cast<uint16_t>(pal[p] | (pal[p + 1] << 8));
                rgba[i * 4 + 0] = static_cast<uint8_t>((entry & 0x1f) << 3);
                rgba[i * 4 + 1] = static_cast<uint8_t>(((entry >> 5) & 0x1f) << 3);
                rgba[i * 4 + 2] = static_cast<uint8_t>(((entry >> 10) & 0x1f) << 3);
                rgba[i * 4 + 3] = 255;
            }
            return rgba;
        }

        std::map<int, TypeArt> byType;
    };
}
