#pragma once

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

        void Unload();
        bool Ready() const { return texture != nullptr; }
        int CurrentWeapon() const { return loadedWeapon; }

        ~WeaponView() { Unload(); }

        // File stem for a weapon index, or empty if out of range.
        static const char* WeaponFileStem(int weaponIndex);

    private:
        SDL_Texture* texture = nullptr;
        int frameWidth = 0;
        int frameHeight = 0;
        int loadedWeapon = -1;
    };
}
