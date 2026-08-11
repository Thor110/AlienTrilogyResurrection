#include "WeaponView.h"

#include "HudPanel.h"

#include <algorithm>

namespace ALTEngine::Renderer
{
    namespace
    {
        // Weapon order is the original's 0-4 (see FUN_0003aac8's ammo switch),
        // which is also the order PlayerInventoryState declares them in.
        //
        // MM9 is the 9mm pistol the player always starts with.
        constexpr const char* WEAPON_FILES[] = { "MM9", "SHOTGUN", "FLAME", "PULSE", "SMART" };
        constexpr int WEAPON_COUNT = static_cast<int>(sizeof(WEAPON_FILES) / sizeof(WEAPON_FILES[0]));

        // Held pose. Section 1 frame 0, with section 0 as a fallback - see the
        // header for why.
        constexpr int RESTING_SECTION = 1;
        constexpr int RESTING_FRAME = 0;
        constexpr int FALLBACK_SECTION = 0;
    }

    const char* WeaponView::WeaponFileStem(int weaponIndex)
    {
        if (weaponIndex < 0 || weaponIndex >= WEAPON_COUNT) { return ""; }
        return WEAPON_FILES[weaponIndex];
    }

    bool WeaponView::SetWeapon(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory,
                               int weaponIndex)
    {
        if (weaponIndex == loadedWeapon && texture) { return true; }
        if (weaponIndex < 0 || weaponIndex >= WEAPON_COUNT) { Unload(); return false; }

        Unload();

        const char* stem = WEAPON_FILES[weaponIndex];
        std::filesystem::path path = cdDirectory / "GFX" / (std::string(stem) + ".B16");

        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec))
        {
            SDL_Log("WeaponView: %s not found - weapon %d will not draw", path.string().c_str(), weaponIndex);
            return false;
        }

        std::optional<ALTEngine::Formats::SpriteFrameInfo> frame;
        int usedSection = RESTING_SECTION;
        for (int attempt = 0; attempt < 2 && !frame; ++attempt)
        {
            usedSection = (attempt == 0) ? RESTING_SECTION : FALLBACK_SECTION;
            try
            {
                frame = ALTEngine::Formats::SpriteFrameLoader::LoadFrame(path, stem, usedSection, RESTING_FRAME);
            }
            catch (const std::exception& e)
            {
                // Not fatal - the weapon just does not draw, which is far better
                // than a crash mid-level.
                SDL_Log("WeaponView: %s section %d frame %d failed to load: %s",
                        stem, usedSection, RESTING_FRAME, e.what());
            }
        }

        if (!frame || frame->width <= 0 || frame->height <= 0 || frame->rgba.empty())
        {
            SDL_Log("WeaponView: %s produced no image for section %d or %d",
                    stem, RESTING_SECTION, FALLBACK_SECTION);
            return false;
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                    frame->width, frame->height);
        if (!texture)
        {
            SDL_Log("WeaponView: could not create texture for %s: %s", stem, SDL_GetError());
            return false;
        }
        SDL_UpdateTexture(texture, nullptr, frame->rgba.data(), frame->width * 4);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        // Point sampling, like the rest of the HUD - these are small pixel-art
        // frames blown up to the window.
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

        frameWidth = frame->width;
        frameHeight = frame->height;
        loadedWeapon = weaponIndex;

        SDL_Log("WeaponView: loaded %s section %d frame %d - %dx%d",
                stem, usedSection, RESTING_FRAME, frameWidth, frameHeight);
        return true;
    }

    void WeaponView::Unload()
    {
        if (texture) { SDL_DestroyTexture(texture); texture = nullptr; }
        frameWidth = 0;
        frameHeight = 0;
        loadedWeapon = -1;
    }

    void WeaponView::Draw(SDL_Renderer* renderer, int outputWidth, int outputHeight) const
    {
        if (!texture || frameWidth <= 0 || frameHeight <= 0) { return; }
        if (outputWidth <= 0 || outputHeight <= 0) { return; }

        // Same 320x240 virtual space and per-axis scaling as the HUD, so the
        // weapon and the HUD keep the same relationship at any resolution.
        float scaleX = static_cast<float>(outputWidth) / static_cast<float>(HUD_VIRTUAL_WIDTH);
        float scaleY = static_cast<float>(outputHeight) / static_cast<float>(HUD_VIRTUAL_HEIGHT);

        // Bottom-centre, sitting on the bottom edge. The ammo row occupies
        // y 211..229, so the sprite is allowed to overlap it - it does in the
        // original too.
        float x = (HUD_VIRTUAL_WIDTH - frameWidth) * 0.5f;
        float y = static_cast<float>(HUD_VIRTUAL_HEIGHT - frameHeight);

        SDL_FRect dst{ SDL_roundf(x * scaleX), SDL_roundf(y * scaleY),
                       SDL_roundf(frameWidth * scaleX), SDL_roundf(frameHeight * scaleY) };
        SDL_RenderTexture(renderer, texture, nullptr, &dst);
    }
}
