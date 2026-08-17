#pragma once

#include "LanguagePack.h"
#include "Localization.h"
#include "PlatformPaths.h"
#include "StringId.h"
#include "StringKeyName.h"
#include "Strings/Strings_English.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ALTEngine::Bootstrap
{
    // Where LoadedLanguagePacks() looks for LANGUAGE/ - defaults to
    // next to the executable (matching Config's own convention) until
    // SetLanguagePackDirectory is called. Edward, 2026: "move the
    // LANGUAGE folder to GameData\CD\LANGUAGE so that it sits alongside
    // the original language files" - main.cpp calls this once, early,
    // right after cdDirectory is known, before anything needs Tr().
    inline std::filesystem::path& LanguagePackDirectoryPath()
    {
        static std::filesystem::path path = ExecutableDirectory() / "LANGUAGE";
        return path;
    }

    inline void SetLanguagePackDirectory(const std::filesystem::path& cdDirectory)
    {
        LanguagePackDirectoryPath() = cdDirectory / "LANGUAGE";
    }

    // Discovered once, cached for the process lifetime (Edward, 2026:
    // "scans the LANGUAGE folder for the localisation files... release
    // language packs rather than bundle them all together"). Packs
    // live in LANGUAGE/ under the game's own CD directory, alongside
    // the original disc's own language-specific files (MISSION#.TXT,
    // FINTRO.AVI, etc) - see SetLanguagePackDirectory above.
    inline std::vector<LanguagePack>& LoadedLanguagePacks()
    {
        static std::vector<LanguagePack> packs = DiscoverLanguagePacks(LanguagePackDirectoryPath());
        return packs;
    }

    // Maps the fixed Language enum to its pack's folder name. The menu
    // still shows a fixed set of language buttons for now - a fully
    // dynamic, discovered-count menu (so an arbitrary new pack shows up
    // as a button automatically) is real future work, not something
    // this pass solves - but the underlying DATA layer below this is
    // already genuinely file-based and open-ended (Edward, 2026: "that
    // isn't for you to worry about just ensure the system is modular
    // and accepts new languages").
    inline const char* LanguageFolderName(Language language)
    {
        switch (language)
        {
        case Language::French:   return "French";
        case Language::Italian:  return "Italian";
        case Language::Spanish:  return "Spanish";
        case Language::German:   return "German";
        case Language::Japanese: return "Japanese";
        case Language::English:
        default: return "English";
        }
    }

    inline const LanguagePack* FindLanguagePack(const std::string& folderName)
    {
        for (const auto& pack : LoadedLanguagePacks())
        {
            if (pack.folderName == folderName) { return &pack; }
        }
        return nullptr;
    }

    // The central lookup for every translatable UI string. Fallback
    // chain, in order: (1) the selected language's own on-disk pack,
    // (2) the on-disk English pack, (3) the compiled-in StringsEnglish()
    // array. Step 3 is the important one for robustness - if LANGUAGE/
    // is missing, deleted, or a pack's files are corrupted, the game
    // still shows correct English text everywhere rather than blank
    // strings throughout the UI (Edward, 2026: "simple and robust").
    //
    // Takes `language` explicitly rather than reading some global
    // "current language" - matches how Language is already threaded
    // through the rest of the engine as a parameter, not a global.
    // Where the port's own key and the ORIGINAL's say the same thing, the
    // original's wins.
    //
    // The .BIN extraction turned out to overlap the port's menu text heavily -
    // "Difficulty", "Camera Sway", the weapon names, the difficulty tier names,
    // Yes/No/On/Off. Rather than renumber every menu node, the port's key is
    // aliased onto the Gs one, so there is a single string to translate and the
    // Gs keys stay the canonical set with their original index recorded.
    //
    // MATCHED ON EXACT TEXT, deliberately. A normalised match found 56 pairs but
    // several were case variants the menus rely on - CreditsTitle is "CREDITS"
    // against GsCredits "Credits", and aliasing those would have quietly
    // lower-cased three menu headings. Requiring the text to match byte for byte
    // leaves 49, and every one is a genuine duplicate.
    //
    // Note the two that look odd but are right: OptionsTitle -> GsOptions (both
    // "OPTIONS") and Options -> GsOptionsMenu (both "Options"). The original
    // happens to carry the same word twice in different cases too.
    inline StringId AliasToOriginal(StringId id)
    {
        struct Alias { StringId own; StringId original; };
        static const Alias aliases[] = {
            { StringId::AcidReign, StringId::GsDifficultyEasy },
            { StringId::ActionFire1, StringId::GsFire1 },
            { StringId::ActionFire2, StringId::GsFire2 },
            { StringId::ActionNextWeapon, StringId::GsNextWeapon },
            { StringId::ActionStrafeLeft, StringId::GsStrafeLeft },
            { StringId::ActionStrafeModifier, StringId::GsStrafeModifier },
            { StringId::ActionStrafeRight, StringId::GsStrafeRight },
            { StringId::AutoMapper, StringId::GsAutoMapper },
            { StringId::Batteries, StringId::GsBatteries },
            { StringId::CameraSway, StringId::GsCameraSway },
            { StringId::Controls, StringId::GsControls },
            { StringId::Credits, StringId::GsCredits },
            { StringId::Difficulty, StringId::GsDifficulty },
            { StringId::ExitGameTitle, StringId::GsExitGame },
            { StringId::Flamethrower, StringId::GsWeaponFlamethrower },
            { StringId::GravisGrip, StringId::GsGravisGrip },
            { StringId::GravisPad, StringId::GsGravisPad },
            { StringId::Joystick, StringId::GsJoystick },
            { StringId::Keyboard, StringId::GsKeyboard },
            { StringId::LanguageEnglish, StringId::GsLangEnglish },
            { StringId::LanguageMenuTitle, StringId::GsLanguage },
            { StringId::LoadGame, StringId::GsLoadGame },
            { StringId::Mission, StringId::GsMission },
            { StringId::Mouse, StringId::GsMouse },
            { StringId::Multiplayer, StringId::GsMultiplayer },
            { StringId::Music, StringId::GsMusic },
            { StringId::No, StringId::GsNo },
            { StringId::NotAvailable, StringId::GsNotAvailable },
            { StringId::Off, StringId::GsOff },
            { StringId::On, StringId::GsOn },
            { StringId::Options, StringId::GsOptionsMenu },
            { StringId::OptionsTitle, StringId::GsOptions },
            { StringId::Pistol9mm, StringId::GsWeapon9mmPistol },
            { StringId::PressEnterToSelect, StringId::GsPressEnterToSelect },
            { StringId::PressEscToGoBack, StringId::GsPressEscToGoBack },
            { StringId::PulseRifle, StringId::GsWeaponPulseRifle },
            { StringId::RagingTerror, StringId::GsDifficultyMedium },
            { StringId::Redefine, StringId::GsRedefine },
            { StringId::SaveGame, StringId::GsSaveGame },
            { StringId::Sfx, StringId::GsSfx },
            { StringId::Shotgun, StringId::GsWeaponShotgun },
            { StringId::ShoulderLamp, StringId::GsShoulderLamp },
            { StringId::SmartGun, StringId::GsWeaponSmartGun },
            { StringId::SpaceOrb360, StringId::GsSpaceOrb },
            { StringId::StartGame, StringId::GsStartGame },
            { StringId::Vfx1, StringId::GsVfx1 },
            { StringId::Volume, StringId::GsVolume },
            { StringId::Xenomania, StringId::GsDifficultyHard },
            { StringId::Yes, StringId::GsYes },
        };
        for (const Alias& alias : aliases)
        {
            if (alias.own == id) { return alias.original; }
        }
        return id;
    }

    inline std::string Tr(StringId id, Language language)
    {
        // Guard the range before anything indexes the table. StringId::Count is
        // one past the end, and any node that ends up carrying it - or a bogus
        // value cast from an int - would otherwise read out of bounds, which is
        // a hard crash in a debug build rather than a quiet wrong string.
        if (static_cast<std::size_t>(id) >= static_cast<std::size_t>(StringId::Count))
        {
            return "";
        }

        // Try the port's own key first, then the original's equivalent. That
        // order matters: a pack that deliberately overrides the port's key keeps
        // working, and only keys with no entry of their own fall through to the
        // shared original text.
        const std::string key = StringKeyName(id);
        const StringId aliased = AliasToOriginal(id);
        const std::string originalKey = (aliased == id) ? std::string() : StringKeyName(aliased);

        if (const LanguagePack* pack = FindLanguagePack(LanguageFolderName(language)))
        {
            if (auto value = pack->Get(key)) { return *value; }
            if (!originalKey.empty())
            {
                if (auto value = pack->Get(originalKey)) { return *value; }
            }
        }

        if (language != Language::English)
        {
            if (const LanguagePack* englishPack = FindLanguagePack("English"))
            {
                if (auto value = englishPack->Get(key)) { return *value; }
            }
        }

        const char* builtIn = StringsEnglish()[static_cast<std::size_t>(id)];
        return builtIn ? builtIn : "";
    }
}
