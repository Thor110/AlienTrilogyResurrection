#pragma once

#include "../Formats/SpriteAnimator.h"
#include "../Formats/SpriteFrameLoader.h"
#include "../Screens/PlayerInventoryState.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Renderer
{
    // The weapon the player is holding, drawn over the world.
    //
    // Each weapon is its own .B16 in CD/GFX with F0## sections of compressed
    // 8bpp frames; SpriteFrameLoader turns one into RGBA, using the frame size
    // table in SpriteFrameDimensions.cpp (transcribed from Edward's ALTViewer
    // and cross-checked entry by entry against it).
    //
    // HOW THE ORIGINAL DRIVES THIS (partly traced):
    //   FUN_000401d0(weaponIndex) selects a weapon. Per weapon it stores
    //     DAT_000b0aa8 = a table of 8-byte entries at fix_off32_000ace58 /
    //                    ace48 / ace28 / ace88 ... - one per weapon STATE,
    //                    each holding a frame-list pointer
    //     DAT_000b0a88 = the weapon's loaded graphic
    //   then FUN_00028a6c(&DAT_000b0a68) starts an animation on a sprite
    //   instance. So the weapon runs through the SAME generic sprite/animation
    //   system the entities use, indexed by weapon state - which is where the
    //   firing and reload sequences live.
    //
    // THE ANIMATOR IS NOW TRACED - see Formats/SpriteAnimator.h, which is a
    // faithful transcription of FUN_00028a6c and its opcode interpreter
    // FUN_000288e0. FUN_000400fc confirms the setup this class mirrors: the
    // instance base is DAT_000b0a68, the per-weapon table at DAT_000b0aa8 holds
    // 8-byte entries of {frame table base, sequence pointer}, and the sequence
    // opens with the frame duration.
    //
    // WHAT IS NOT TRACED IS THE SEQUENCE DATA ITSELF. The tables live in the
    // game files and are not parsed yet, so the idle and fire sequences below
    // are SYNTHESISED from the frames each weapon's section actually contains.
    // They run on the real VM, so replacing them with the original's own
    // sequence bytes later is a data change, not a code change.
    //
    // WHICH SECTION IS THE HELD POSE - a hypothesis, not traced. Section 1
    // frame 0 is used, not section 0. Reasoning: MM9's section 0 is 40x68 while
    // section 1 frame 0 is 40x88, and measuring the pistol in a screenshot of
    // the original gives roughly 90 units tall in 320x240 space - which matches
    // section 1 frame 0 at 1:1 and not section 0. That also gives section 0 a
    // sensible job as the pickup/inventory icon, and makes MM9's section 1
    // (40x88, 40x72, 40x68) read as a recoil sequence. Falls back to section 0
    // if section 1 will not load.
    //
    // PLACEMENT IS STILL OURS. The sprite is anchored bottom-centre in the
    // 320x240 HUD space. The original's own position comes from that sprite
    // instance and has not been read out yet.

    class WeaponView
    {
    public:
        // Loads the sprite for a weapon index (0-4, matching the original's
        // order and the inventory's declaration order). Cheap to call every
        // frame; only reloads when the weapon changes.
        bool SetWeapon(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory, int weaponIndex);

        void Draw(SDL_Renderer* renderer, int outputWidth, int outputHeight) const;

        // Advances the animation by one of the ORIGINAL'S logic ticks, not one
        // frame - call it from the same fixed-rate loop the player runs on, or
        // the firing speed changes with the frame rate.
        void Tick();

        // Starts the firing animation. Ignored while one is already playing, so
        // holding the trigger does not restart it every tick; that gives a
        // repeat rate set by the sequence length, which is the shape the
        // original has even though its own rate is not read out yet.
        void Fire();

        bool IsFiring() const { return firing; }

        // True on the tick an OP_EVENT opcode fired, with its operand. This is
        // where a muzzle flash, a shot, or a sound would hang once the meaning
        // of the operand is known - see SpriteAnimator.h.
        bool EventFired() const { return animator.EventFired(); }
        int EventParam() const { return static_cast<int>(animator.param); }

        void Unload();
        bool Ready() const { return !frames.empty(); }
        int CurrentWeapon() const { return loadedWeapon; }
        int FrameCount() const { return static_cast<int>(frames.size()); }

        ~WeaponView() { Unload(); }

        // File stem for a weapon index, or empty if out of range.
        static const char* WeaponFileStem(int weaponIndex);

    private:
        struct FrameTexture
        {
            SDL_Texture* texture = nullptr;
            int width = 0;
            int height = 0;
        };

        // Every frame of the weapon's animation section, in file order. The
        // animator's frame index selects one.
        std::vector<FrameTexture> frames;
        int loadedWeapon = -1;

        ALTEngine::Formats::SpriteAnim::Animator animator;
        std::vector<uint16_t> idleSequence;
        std::vector<uint16_t> fireSequence;
        bool firing = false;

        void StartIdle();
    };
}
