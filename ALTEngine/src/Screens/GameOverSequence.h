#pragma once

#include "../Bootstrap/Localization.h"
#include "../Video/OverrideVideo.h"
#include "../Video/VideoPlayer.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Screens
{
    // The game over sequence.
    //
    // WHAT THE FILES SAY. The disc carries DEATH1.AVI through DEATH6.AVI and
    // GAMEOVER.AVI, and the override folder already holds .mp4 versions of the
    // same names - so both parts exist and both are meant to play (Edward, 2026).
    //
    // WHAT THE CODE SAYS: nothing. There is no "DEATH" or "GAMEOVER" string
    // anywhere in the image, and no .AVI filename at all - only the -nomovies
    // switch text. The executable builds its video paths at runtime rather than
    // storing them, so the sequence cannot be read out the way the level and
    // sprite tables could. What is here is therefore built from the FILES, and
    // the parts that are guesses are marked as such.
    //
    // IT IS NOT RANDOM - THE VIDEO IS CHOSEN BY HOW YOU DIED, exactly as Edward
    // said. Found by following the second argument of the player damage call:
    //
    //   FUN_0003e5a8(damage, cause)   the entry every attacker uses
    //   FUN_0003e4a8                  stores `cause` via FUN_00015ab4 into
    //                                 DAT_0023e7dc, the death-cause variable
    //   FUN_00015a68                  on game over, maps that cause to a video
    //                                 and calls FUN_000522a0 to play it
    //
    // The map is a plain switch:
    //
    //     cause 0 -> video 3       cause 3 -> video 5
    //     cause 1 -> video 6       cause 4 -> video 2
    //     cause 2 -> video 4       anything else -> video 7
    //
    // Those video ids run 2..7 rather than 1..6, so FUN_000522a0's id space
    // covers more than the death clips and DEATH1..DEATH6 sit at ids 2..7 -
    // giving DEATH(id - 1).
    //
    // AND THE CAUSES ARE KNOWN from what the callers pass:
    //     FUN_00033ff8 -> cause 4   an enemy's attack     -> DEATH1
    //     FUN_000368c8 -> cause 5   an explosion          -> DEATH6 (the default)
    //     cause 3 is the one FUN_0003e4a8 singles out for a different sound when
    //     the player has armour                           -> DEATH4
    //
    // So being mauled and being blown up give different clips, which is the whole
    // point of having six.
    //
    // STILL UNVERIFIED: whether GAMEOVER.AVI plays after the death clip.
    // FUN_00015a68 plays exactly one video and does not chain a second, so on the
    // evidence it may be that GAMEOVER belongs to a different path - quitting, or
    // running out of lives - rather than to a single death. Kept in the sequence
    // because Edward thought it played, and marked so it is easy to drop.
    namespace GameOverSequence
    {
        inline constexpr int DEATH_VIDEO_COUNT = 6;

        // Damage cause codes, from what the callers of FUN_0003e5a8 pass.
        enum DeathCause
        {
            CAUSE_UNKNOWN_0 = 0,
            CAUSE_UNKNOWN_1 = 1,
            CAUSE_UNKNOWN_2 = 2,
            CAUSE_ARMOUR_HIT = 3,   // the one FUN_0003e4a8 gives its own sound
            CAUSE_ENEMY_ATTACK = 4, // FUN_00033ff8
            CAUSE_EXPLOSION = 5,    // FUN_000368c8
        };

        // The cause-to-video map, transcribed from FUN_00015a68. Returns the
        // one-based DEATH clip number.
        inline int DeathVideoForCause(int cause)
        {
            int videoId = 7;   // the switch's default
            switch (cause)
            {
            case 0: videoId = 3; break;
            case 1: videoId = 6; break;
            case 2: videoId = 4; break;
            case 3: videoId = 5; break;
            case 4: videoId = 2; break;
            default: break;
            }
            // Ids 2..7 are DEATH1..DEATH6.
            return videoId - 1;
        }
        inline constexpr const char* GAME_OVER_BASE_NAME = "GAMEOVER";

        // "DEATH1" .. "DEATH6", one-based as the files are.
        inline std::string DeathVideoBaseName(int index)
        {
            return "DEATH" + std::to_string(index);
        }

        // Plays one video by base name, override first, then the localised .AVI.
        // Returns false only if the window was closed - a missing file is skipped,
        // the same philosophy the intro sequence uses.
        inline bool PlayOne(const std::filesystem::path& cdDirectory,
                            ALTEngine::Bootstrap::Language language,
                            const std::string& baseName)
        {
            if (auto overridePath = ALTEngine::Video::FindOverrideVideo(cdDirectory, baseName))
            {
                return ALTEngine::Video::VideoPlayer::Play(*overridePath);
            }

            const std::string localized = ALTEngine::Bootstrap::LocalizedBaseName(baseName, language);
            const std::filesystem::path path = cdDirectory / "AVI" / (localized + ".AVI");
            std::error_code ec;
            if (!std::filesystem::exists(path, ec)) { return true; }   // absent, not fatal

            return ALTEngine::Video::VideoPlayer::Play(path);
        }

        // The whole sequence: one of the six deaths, then GAMEOVER.
        //
        // `deathIndex` is one-based; pass 0 to have one picked. The caller supplies
        // the choice so it can be seeded or forced for testing rather than this
        // reaching for a global generator.
        inline bool Play(const std::filesystem::path& cdDirectory,
                         ALTEngine::Bootstrap::Language language,
                         int deathIndex)
        {
            if (deathIndex < 1 || deathIndex > DEATH_VIDEO_COUNT) { deathIndex = 1; }

            if (!PlayOne(cdDirectory, language, DeathVideoBaseName(deathIndex))) { return false; }
            return PlayOne(cdDirectory, language, GAME_OVER_BASE_NAME);
        }
    }
}
