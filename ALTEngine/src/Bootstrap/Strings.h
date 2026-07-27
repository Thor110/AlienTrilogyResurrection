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
    inline std::string Tr(StringId id, Language language)
    {
        std::string key = StringKeyName(id);

        if (const LanguagePack* pack = FindLanguagePack(LanguageFolderName(language)))
        {
            if (auto value = pack->Get(key)) { return *value; }
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
