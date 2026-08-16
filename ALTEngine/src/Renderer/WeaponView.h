#pragma once

#include "../Formats/SpriteAnimator.h"
#include "../Screens/WeaponSystem.h"
#include "../Formats/SpriteFrameLoader.h"
#include "../Screens/PlayerInventoryState.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <string>
#include <array>
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
    // A .B16 SECTION IS A WEAPON STATE. Section 0 is idle, section 1 is firing,
    // section 2 is reloading, and the section's sub-frame count is the length of
    // that state's animation (Edward, 2026 - measured from the files).
    //
    // The pistol's own state table at 0x000ace58 agrees exactly. The gap between
    // each state's frame table and its sequence pointer is the table itself, at
    // 12 bytes per record: idle 12 bytes = 1 frame, firing 36 = 3, reload
    // 36 = 3. MM9's sections have 1, 3 and 3 sub-frames. The section index and
    // the state index are the same number.
    //
    // An earlier version of this guessed that section 1 frame 0 was the held
    // pose, from a screenshot measurement. It was wrong - that is the first
    // frame of the FIRING animation, which is why the weapon appeared to rest
    // in a recoiled position.
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
        //
        // Returns true when the sequence has just ENDED, which is the original's
        // cue to drop the weapon back to idle (FUN_0003e93c tests the animator's
        // flag 2 for exactly this).
        bool Tick();

        // Plays the sequence for a weapon state. This is the original's
        // FUN_000400fc: the state indexes the per-weapon table and starting its
        // animation IS the state change.
        void PlayState(int weaponState);

        // The camera's own bob term, so the weapon rides it - see
        // WeaponSystem::WeaponBobOffset.
        void SetCameraDip(int dip) { cameraDip = dip; }

        // How far down the weapon is drawn while a switch is in progress - see
        // WeaponSystem::SwitchAnimation.
        void SetSwitchOffset(int rows) { switchOffset = rows; }

        // True on the tick the animation asked for a sound, with its id.
        bool SoundCued() const { return animator.EventFired(); }
        int CuedSoundId() const { return static_cast<int>(animator.param); }

        // True on the tick an OP_EVENT opcode fired, with its operand. This is
        // where a muzzle flash, a shot, or a sound would hang once the meaning
        // of the operand is known - see SpriteAnimator.h.
        bool EventFired() const { return animator.EventFired(); }
        int EventParam() const { return static_cast<int>(animator.param); }

        void Unload();
        bool Ready() const { return !frames[ALTEngine::Screens::WeaponSystem::STATE_IDLE].empty(); }
        int CurrentWeapon() const { return loadedWeapon; }
        int FrameCount(int weaponState) const
        {
            if (weaponState < 0 || weaponState >= ALTEngine::Screens::WeaponSystem::STATE_COUNT) { return 0; }
            return static_cast<int>(frames[static_cast<size_t>(weaponState)].size());
        }

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

        // Frames per weapon STATE. A .B16 section maps one-to-one onto a
        // weapon state, so section 0 is idle, section 1 is firing, section 2 is
        // reload - see the header. The animator's frame index selects within
        // the current state's section.
        std::array<std::vector<FrameTexture>,
                   ALTEngine::Screens::WeaponSystem::STATE_COUNT> frames;
        int loadedWeapon = -1;

        ALTEngine::Formats::SpriteAnim::Animator animator;

        // One sequence per weapon state, indexed by WeaponSystem::State.
        //
        // The ORIGINAL has one per state too - that is exactly what its 8-byte
        // per-state table entries are. What it does NOT share is the contents:
        // its sequences are data this cannot read yet, so these are synthesised
        // from the frames the weapon's section actually has. See the header.
        std::array<std::vector<uint16_t>,
                   ALTEngine::Screens::WeaponSystem::STATE_COUNT> stateSequences;
        int currentState = ALTEngine::Screens::WeaponSystem::STATE_IDLE;
        int cameraDip = 0;
        int switchOffset = 0;

        void BuildSequences();
    };
}
