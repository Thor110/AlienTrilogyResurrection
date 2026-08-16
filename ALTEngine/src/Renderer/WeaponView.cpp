#include "WeaponView.h"
#include "WeaponData.h"

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

        // Section index == weapon state index. See the header.
        //
        // Sections beyond the ones a weapon actually has simply fail to load and
        // are left empty, which is correct: the shotgun has no reload state and
        // its state table has no entry for one either.
        constexpr int MAX_SECTION = ALTEngine::Screens::WeaponSystem::STATE_COUNT;

        // No weapon section is anywhere near this long; the cap only stops the
        // probe loop running away if LoadFrame ever starts succeeding for
        // out-of-range indices.
        constexpr int MAX_FRAMES = 32;

        // All five weapons' frame offsets and sequences now come from
        // Renderer/WeaponData.h, transcribed from the data block at 0x090000.
        // Nothing in this file is synthesised any more.

        // FRAME DURATIONS - CONFIRMED for the pistol, read from its own state
        // table at 0x000ace58. Each entry is {frame table, sequence}, and the
        // sequence's first word is the duration:
        //
        //   state 0 idle    frames 0x090160  sequence 0x09016c  duration 4
        //   state 1 firing  frames 0x090180  sequence 0x0901a4  duration 2
        //   state 2 reload  frames 0x0901c0  sequence 0x0901e4  duration 3
        //
        // The gap between a state's frame table and its sequence is the table
        // itself, at 12 bytes per record - so idle is ONE frame, firing is
        // THREE, and reload is THREE. That matches MM9's section 1 having three
        // frames, and it means idle has its own single-entry frame table rather
        // than borrowing one of the firing frames.
        //
        // WHICH sprite frame that single idle record points at is the one thing
        // still missing, and it is why the held pose starts on the wrong image.
        // The 12 bytes at 0x090160 answer it.
        constexpr uint16_t IDLE_DURATION = 4;
        constexpr uint16_t FIRE_DURATION = 2;
        constexpr uint16_t RELOAD_DURATION = 3;
    }

    const char* WeaponView::WeaponFileStem(int weaponIndex)
    {
        if (weaponIndex < 0 || weaponIndex >= WEAPON_COUNT) { return ""; }
        return WEAPON_FILES[weaponIndex];
    }

    bool WeaponView::SetWeapon(SDL_Renderer* renderer, const std::filesystem::path& cdDirectory,
                               int weaponIndex)
    {
        if (weaponIndex == loadedWeapon && !frames[ALTEngine::Screens::WeaponSystem::STATE_IDLE].empty()) { return true; }
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

        // One section per state, sub-frames within it. A section that will not
        // load leaves that state empty, which is how a weapon without a reload
        // is expressed - no special-casing needed.
        int totalFrames = 0;
        for (int section = 0; section < MAX_SECTION; ++section)
        {
            for (int frameIndex = 0; frameIndex < MAX_FRAMES; ++frameIndex)
            {
                std::optional<ALTEngine::Formats::SpriteFrameInfo> frame;
                try
                {
                    frame = ALTEngine::Formats::SpriteFrameLoader::LoadFrame(path, stem, section, frameIndex);
                }
                catch (const std::exception& e)
                {
                    // Not fatal - the state just stops at the frames it got,
                    // which is far better than a crash mid-level.
                    SDL_Log("WeaponView: %s section %d frame %d failed to load: %s",
                            stem, section, frameIndex, e.what());
                    break;
                }

                if (!frame || frame->width <= 0 || frame->height <= 0 || frame->rgba.empty()) { break; }

                FrameTexture ft;
                ft.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                               frame->width, frame->height);
                if (!ft.texture)
                {
                    SDL_Log("WeaponView: could not create texture for %s: %s", stem, SDL_GetError());
                    Unload();
                    return false;
                }
                SDL_UpdateTexture(ft.texture, nullptr, frame->rgba.data(), frame->width * 4);
                SDL_SetTextureBlendMode(ft.texture, SDL_BLENDMODE_BLEND);
                // Point sampling, like the rest of the HUD - these are small
                // pixel-art frames blown up to the window.
                SDL_SetTextureScaleMode(ft.texture, SDL_SCALEMODE_NEAREST);
                ft.width = frame->width;
                ft.height = frame->height;
                frames[static_cast<size_t>(section)].push_back(ft);
                totalFrames++;
            }
        }

        if (frames[ALTEngine::Screens::WeaponSystem::STATE_IDLE].empty())
        {
            SDL_Log("WeaponView: %s has no section 0 - nothing to hold", stem);
            Unload();
            return false;
        }

        loadedWeapon = weaponIndex;
        BuildSequences();
        PlayState(ALTEngine::Screens::WeaponSystem::STATE_IDLE);

        SDL_Log("WeaponView: loaded %s - %d frames across %d/%d/%d (idle/fire/reload), first %dx%d",
                stem, totalFrames,
                FrameCount(ALTEngine::Screens::WeaponSystem::STATE_IDLE),
                FrameCount(ALTEngine::Screens::WeaponSystem::STATE_FIRING),
                FrameCount(ALTEngine::Screens::WeaponSystem::STATE_RELOAD),
                frames[ALTEngine::Screens::WeaponSystem::STATE_IDLE].front().width,
                frames[ALTEngine::Screens::WeaponSystem::STATE_IDLE].front().height);
        return true;
    }

    void WeaponView::BuildSequences()
    {
        namespace WS = ALTEngine::Screens::WeaponSystem;
        for (auto& seq : stateSequences) { seq.clear(); }

        auto framesOf = [&](int state) {
            std::vector<uint16_t> list;
            for (size_t i = 0; i < frames[static_cast<size_t>(state)].size(); ++i)
            {
                list.push_back(static_cast<uint16_t>(i));
            }
            return list;
        };

        // Idle holds its single frame forever - OP_LOOP with an operand of 0
        // reseeds a zero counter every pass and so never terminates.
        stateSequences[WS::STATE_IDLE] =
            ALTEngine::Formats::SpriteAnim::BuildSequence(IDLE_DURATION, framesOf(WS::STATE_IDLE), true);

        // Firing and reloading run their section once and stop on the last
        // frame. FLAG_ENDED is then what returns the weapon to idle, which is
        // exactly how FUN_0003e93c does it.
        //
        // A state with no frames of its own still gets a sequence, so the state
        // machine keeps its timing - it just holds the idle image while it runs.
        for (int state : { WS::STATE_FIRING, WS::STATE_RELOAD, WS::STATE_GRENADE })
        {
            std::vector<uint16_t> list = framesOf(state);
            const uint16_t duration = (state == WS::STATE_RELOAD) ? RELOAD_DURATION : FIRE_DURATION;
            if (list.empty()) { list.push_back(0); }
            stateSequences[static_cast<size_t>(state)] =
                ALTEngine::Formats::SpriteAnim::BuildSequence(duration, list, false);
        }

        // Replace the generated sequences with the real ones wherever the
        // weapon's frame count matches what the original's table expects. The
        // guard matters: if a .B16 section has fewer frames than the sequence
        // indexes, the animator would run off the end of the loaded textures.
        const WeaponData::WeaponTables& tables = WeaponData::For(loadedWeapon);
        for (int state = 0; state < 4; ++state)
        {
            const std::vector<uint16_t>* seq = tables.sequences[state];
            if (!seq) { continue; }
            if (FrameCount(state) < static_cast<int>(tables.offsetCounts[state])) { continue; }
            stateSequences[static_cast<size_t>(state)] = *seq;
        }
    }

    void WeaponView::PlayState(int weaponState)
    {
        if (weaponState < 0 || weaponState >= ALTEngine::Screens::WeaponSystem::STATE_COUNT) { return; }
        currentState = weaponState;
        ALTEngine::Formats::SpriteAnim::Start(animator, stateSequences[static_cast<size_t>(weaponState)]);
    }

    bool WeaponView::Tick()
    {
        if (frames[ALTEngine::Screens::WeaponSystem::STATE_IDLE].empty()) { return false; }

        const std::vector<uint16_t>& sequence = stateSequences[static_cast<size_t>(currentState)];
        const bool wasEnded = animator.Ended();
        ALTEngine::Formats::SpriteAnim::Tick(animator, sequence);
        return animator.Ended() && !wasEnded;
    }

    void WeaponView::Unload()
    {
        for (auto& section : frames)
        {
            for (FrameTexture& ft : section)
            {
                if (ft.texture) { SDL_DestroyTexture(ft.texture); }
            }
            section.clear();
        }
        for (auto& seq : stateSequences) { seq.clear(); }
        animator = ALTEngine::Formats::SpriteAnim::Animator{};
        currentState = ALTEngine::Screens::WeaponSystem::STATE_IDLE;
        loadedWeapon = -1;
    }

    void WeaponView::Draw(SDL_Renderer* renderer, int outputWidth, int outputHeight) const
    {
        namespace WSD = ALTEngine::Screens::WeaponSystem;
        if (outputWidth <= 0 || outputHeight <= 0) { return; }

        // Draw from the CURRENT STATE's section. A state with no artwork of its
        // own falls back to the idle pose rather than drawing nothing.
        size_t section = (currentState >= 0 && currentState < WSD::STATE_COUNT)
                       ? static_cast<size_t>(currentState) : 0;
        if (frames[section].empty()) { section = WSD::STATE_IDLE; }
        if (frames[section].empty()) { return; }

        size_t index = animator.frameIndex;
        if (index >= frames[section].size()) { index = 0; }
        const FrameTexture& current = frames[section][index];
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
        const size_t w = (loadedWeapon >= 0 && loadedWeapon < static_cast<int>(WS::WEAPON_OFFSET_X.size()))
                       ? static_cast<size_t>(loadedWeapon) : 0;

        const int anchorX = WS::WEAPON_ANCHOR_X + WS::WEAPON_OFFSET_X[w];
        const int anchorY = WS::WEAPON_ANCHOR_Y + WS::WeaponBobOffset(cameraDip) + WS::WEAPON_Y_BIAS
                          + WS::WEAPON_OFFSET_Y[w];

        // Per-frame offset from the weapon's frame table, where one is known.
        // Anything without real data keeps the old bottom-centre placement,
        // which is a guess and is marked as one.
        const WeaponData::WeaponTables& tables = WeaponData::For(loadedWeapon);
        const WeaponData::FrameOffset* offsets =
            (section < 4) ? tables.offsets[section] : nullptr;
        const size_t offsetCount = (section < 4) ? tables.offsetCounts[section] : 0;

        float x;
        float y;
        if (offsets && index < offsetCount)
        {
            x = static_cast<float>(anchorX + offsets[index].x);
            y = static_cast<float>(anchorY + offsets[index].y);
        }
        else
        {
            // GUESS - no frame table for this weapon yet.
            x = anchorX - frameWidth * 0.5f;
            y = static_cast<float>(anchorY - frameHeight);
        }

        // Recoil now moves the muzzle the right way. With the real offsets the
        // firing frames run (-10,-86), (-7,-67), (-6,-64) - the first frame both
        // taller AND placed higher, so the gun kicks UP and settles back rather
        // than sinking, which is what bottom-anchoring made it do.
        SDL_FRect dst{ SDL_roundf(x * scaleX), SDL_roundf(y * scaleY),
                       SDL_roundf(frameWidth * scaleX), SDL_roundf(frameHeight * scaleY) };
        SDL_RenderTexture(renderer, current.texture, nullptr, &dst);
    }
}
