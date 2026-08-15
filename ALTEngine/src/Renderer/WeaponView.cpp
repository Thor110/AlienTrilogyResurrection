#include "WeaponView.h"

#include "HudPanel.h"

#include <algorithm>
#include <utility>

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

        // No weapon section is anywhere near this long; the cap only stops the
        // probe loop running away if LoadFrame ever starts succeeding for
        // out-of-range indices.
        constexpr int MAX_FRAMES = 32;

        // SYNTHESISED SEQUENCE TIMING - both values are GUESSES.
        //
        // The original's sequences carry their own frame duration in entry 0
        // (FUN_000400fc reads it straight out of the table), and that table is
        // not parsed yet. These are the two numbers to replace first once it is.
        //
        // IDLE_DURATION is irrelevant while idle is a single held frame, but it
        // matters the moment a weapon turns out to have an idle cycle.
        constexpr uint16_t IDLE_DURATION = 4;
        constexpr uint16_t FIRE_DURATION = 2;
        constexpr uint16_t RELOAD_DURATION = 10;
    }

    const char* WeaponView::WeaponFileStem(int weaponIndex)
    {
        if (weaponIndex < 0 || weaponIndex >= WEAPON_COUNT) { return ""; }
        return WEAPON_FILES[weaponIndex];
    }

    bool WeaponView::SetWeapon(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory,
                               int weaponIndex)
    {
        if (weaponIndex == loadedWeapon && !frames.empty()) { return true; }
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

        // Pick the section the same way as before - section 1, falling back to
        // section 0 - but then take EVERY frame it has, not just frame 0. The
        // extra frames are the animation; MM9's section 1 is 40x88, 40x72,
        // 40x68, which reads as a recoil.
        int usedSection = RESTING_SECTION;
        std::vector<ALTEngine::Formats::SpriteFrameInfo> loaded;

        for (int attempt = 0; attempt < 2 && loaded.empty(); ++attempt)
        {
            usedSection = (attempt == 0) ? RESTING_SECTION : FALLBACK_SECTION;
            for (int frameIndex = 0; frameIndex < MAX_FRAMES; ++frameIndex)
            {
                std::optional<ALTEngine::Formats::SpriteFrameInfo> frame;
                try
                {
                    frame = ALTEngine::Formats::SpriteFrameLoader::LoadFrame(path, stem, usedSection, frameIndex);
                }
                catch (const std::exception& e)
                {
                    // Not fatal - the weapon just stops at the frames it got,
                    // which is far better than a crash mid-level.
                    SDL_Log("WeaponView: %s section %d frame %d failed to load: %s",
                            stem, usedSection, frameIndex, e.what());
                    break;
                }

                if (!frame || frame->width <= 0 || frame->height <= 0 || frame->rgba.empty()) { break; }
                loaded.push_back(std::move(*frame));
            }
        }

        if (loaded.empty())
        {
            SDL_Log("WeaponView: %s produced no image for section %d or %d",
                    stem, RESTING_SECTION, FALLBACK_SECTION);
            return false;
        }

        for (const auto& frame : loaded)
        {
            FrameTexture ft;
            ft.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                           frame.width, frame.height);
            if (!ft.texture)
            {
                SDL_Log("WeaponView: could not create texture for %s: %s", stem, SDL_GetError());
                Unload();
                return false;
            }
            SDL_UpdateTexture(ft.texture, nullptr, frame.rgba.data(), frame.width * 4);
            SDL_SetTextureBlendMode(ft.texture, SDL_BLENDMODE_BLEND);
            // Point sampling, like the rest of the HUD - these are small pixel-art
            // frames blown up to the window.
            SDL_SetTextureScaleMode(ft.texture, SDL_SCALEMODE_NEAREST);
            ft.width = frame.width;
            ft.height = frame.height;
            frames.push_back(ft);
        }

        loadedWeapon = weaponIndex;

        // Idle holds frame 0 forever; firing runs every frame once and stops on
        // the last one, at which point Tick() drops back to idle. Both are built
        // on the real VM - see the header on why the DATA is synthesised.
        BuildSequences();
        PlayState(ALTEngine::Screens::WeaponSystem::STATE_IDLE);

        SDL_Log("WeaponView: loaded %s section %d - %d frame(s), first %dx%d",
                stem, usedSection, static_cast<int>(frames.size()),
                frames.front().width, frames.front().height);
        return true;
    }

    void WeaponView::BuildSequences()
    {
        namespace WS = ALTEngine::Screens::WeaponSystem;
        for (auto& seq : stateSequences) { seq.clear(); }

        // Idle holds frame 0 forever - OP_LOOP with an operand of 0 reseeds a
        // zero counter every pass and so never terminates.
        stateSequences[WS::STATE_IDLE] =
            ALTEngine::Formats::SpriteAnim::BuildSequence(IDLE_DURATION, { 0 }, true);

        // Firing runs every frame the section has, once, and stops on the last.
        // FLAG_ENDED is then what returns the weapon to idle, which is exactly
        // how FUN_0003e93c does it.
        std::vector<uint16_t> fireFrames;
        for (int i = 0; i < static_cast<int>(frames.size()); ++i)
        {
            fireFrames.push_back(static_cast<uint16_t>(i));
        }
        stateSequences[WS::STATE_FIRING] =
            ALTEngine::Formats::SpriteAnim::BuildSequence(FIRE_DURATION, fireFrames,
                                                         fireFrames.size() <= 1);

        // Reload, grenade and empty have NO frames of their own here. The
        // original has a distinct sequence for each - that is what its per-state
        // table is for - but which frames they use is in data that is not parsed
        // yet. Running the fire frames backwards for a reload would be inventing
        // animation, so instead these hold frame 0 and simply take time, which
        // keeps the state machine's timing honest without faking artwork.
        const std::vector<uint16_t> holdFrame{ 0 };
        stateSequences[WS::STATE_RELOAD] =
            ALTEngine::Formats::SpriteAnim::BuildSequence(RELOAD_DURATION, holdFrame, false);
        stateSequences[WS::STATE_GRENADE] =
            ALTEngine::Formats::SpriteAnim::BuildSequence(FIRE_DURATION, holdFrame, false);
        stateSequences[WS::STATE_EMPTY] =
            ALTEngine::Formats::SpriteAnim::BuildSequence(IDLE_DURATION, holdFrame, true);
        stateSequences[WS::STATE_UNKNOWN_5] = stateSequences[WS::STATE_IDLE];
    }

    void WeaponView::PlayState(int weaponState)
    {
        if (weaponState < 0 || weaponState >= ALTEngine::Screens::WeaponSystem::STATE_COUNT) { return; }
        currentState = weaponState;
        ALTEngine::Formats::SpriteAnim::Start(animator, stateSequences[static_cast<size_t>(weaponState)]);
    }

    bool WeaponView::Tick()
    {
        if (frames.empty()) { return false; }

        const std::vector<uint16_t>& sequence = stateSequences[static_cast<size_t>(currentState)];
        const bool wasEnded = animator.Ended();
        ALTEngine::Formats::SpriteAnim::Tick(animator, sequence);
        return animator.Ended() && !wasEnded;
    }

    void WeaponView::Unload()
    {
        for (FrameTexture& ft : frames)
        {
            if (ft.texture) { SDL_DestroyTexture(ft.texture); }
        }
        frames.clear();
        for (auto& seq : stateSequences) { seq.clear(); }
        animator = ALTEngine::Formats::SpriteAnim::Animator{};
        currentState = ALTEngine::Screens::WeaponSystem::STATE_IDLE;
        loadedWeapon = -1;
    }

    void WeaponView::Draw(SDL_Renderer* renderer, int outputWidth, int outputHeight) const
    {
        if (frames.empty()) { return; }
        if (outputWidth <= 0 || outputHeight <= 0) { return; }

        size_t index = animator.frameIndex;
        if (index >= frames.size()) { index = 0; }
        const FrameTexture& current = frames[index];
        if (!current.texture || current.width <= 0 || current.height <= 0) { return; }
        const int frameWidth = current.width;
        const int frameHeight = current.height;

        // Same 320x240 virtual space and per-axis scaling as the HUD, so the
        // weapon and the HUD keep the same relationship at any resolution.
        float scaleX = static_cast<float>(outputWidth) / static_cast<float>(HUD_VIRTUAL_WIDTH);
        float scaleY = static_cast<float>(outputHeight) / static_cast<float>(HUD_VIRTUAL_HEIGHT);

        // TRACED. FUN_0003e93c writes the weapon instance's position as
        //     x = 0xa0                                + stateOffsetX
        //     y = 0xf0 - (bob) + DAT_000b0b57         + stateOffsetY
        // 0xa0 is 160 and 0xf0 is 240, so the anchor is bottom-centre of the
        // 320x240 surface - which is what was guessed here before, now
        // confirmed. What is NEW is the bob term: the weapon rides the camera's
        // own dip, from the same phase, moving down up to 4 pixels as the head
        // sinks.
        namespace WS = ALTEngine::Screens::WeaponSystem;
        const int stateIndex = (currentState >= 0 && currentState < WS::STATE_COUNT) ? currentState : 0;

        const int anchorX = WS::WEAPON_ANCHOR_X + WS::STATE_OFFSET_X[static_cast<size_t>(stateIndex)];
        const int anchorY = WS::WEAPON_ANCHOR_Y + WS::WeaponBobOffset(cameraDip) + WS::WEAPON_Y_BIAS
                          + WS::STATE_OFFSET_Y[static_cast<size_t>(stateIndex)];

        float x = anchorX - frameWidth * 0.5f;
        float y = static_cast<float>(anchorY - frameHeight);

        // A shorter recoil frame still pulls the muzzle DOWN rather than
        // kicking it up, because the anchor is the bottom edge. That is what the
        // original's own anchor does too - the correction is the per-state
        // offset table at DAT_000acea2, which is not readable from the export
        // and is currently all zeros. See WeaponSystem.h.
        SDL_FRect dst{ SDL_roundf(x * scaleX), SDL_roundf(y * scaleY),
                       SDL_roundf(frameWidth * scaleX), SDL_roundf(frameHeight * scaleY) };
        SDL_RenderTexture(renderer, current.texture, nullptr, &dst);
    }
}
