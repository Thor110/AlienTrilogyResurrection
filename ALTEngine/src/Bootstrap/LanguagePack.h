#pragma once

#include "KeyValueFile.h"
#include "StringKeyName.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ALTEngine::Bootstrap
{
    // A single language pack, loaded from LANGUAGE/<folder>/ at runtime -
    // NOT compiled into the binary (Edward, 2026: "release language
    // packs rather than bundle them all together... otherwise we will
    // end up having to bake in possibly 500mbs of language packs in the
    // far future"). folderName is the stable on-disk identifier (the
    // subfolder's own name, e.g. "English") - used to select a pack by
    // name without depending on discovery order.
    struct LanguagePack
    {
        std::string folderName;
        std::string displayName;                 // shown on the menu button (e.g. "English", "Français")
        std::string font = "default";            // chosen font identifier - only "default" (the 8x8 bitmap font) exists right now, but the field exists for when a non-Latin script (Japanese, Egyptian Hieroglyphics, etc) needs a different one
        std::string moviePrefix;                  // AVI filename prefix, e.g. "F" for French, empty for English - see Localization.h's LocalizedBaseName
        std::vector<std::string> missionSuffixes; // MISSION#.TXT suffix letters to try, in order, e.g. {"U","E"} for English
        std::unordered_map<std::string, std::string> strings; // StringKeyName(id) -> translated text; missing keys mean "not translated yet"

        // Looks up `key`'s translated text, or std::nullopt if this pack
        // has no entry for it (not yet translated / unrecognised key) -
        // callers (Tr()) fall back to English for std::nullopt, same
        // policy as the old compiled-in tables' nullptr entries had.
        std::optional<std::string> Get(const std::string& key) const
        {
            auto it = strings.find(key);
            if (it == strings.end()) { return std::nullopt; }
            return it->second;
        }
    };

    // Loads a single pack from `folder` (e.g. LANGUAGE/English/) - reads
    // language.cfg for the manifest fields and strings.txt for the text.
    // Returns a pack with just folderName set (everything else default/
    // empty) if either file is missing - matches ParseKeyValueFile's own
    // "missing file isn't an error" policy, since a pack folder that
    // only has one of the two files should still degrade gracefully
    // rather than be skipped entirely.
    inline LanguagePack LoadLanguagePack(const std::filesystem::path& folder)
    {
        LanguagePack pack;
        pack.folderName = folder.filename().string();

        auto manifest = ParseKeyValueFile(folder / "language.cfg");
        if (auto it = manifest.find("DisplayName"); it != manifest.end()) { pack.displayName = it->second; }
        else { pack.displayName = pack.folderName; } // fallback so a pack missing this field still shows *something* readable
        if (auto it = manifest.find("Font"); it != manifest.end()) { pack.font = it->second; }
        if (auto it = manifest.find("MoviePrefix"); it != manifest.end()) { pack.moviePrefix = it->second; }
        if (auto it = manifest.find("MissionSuffixes"); it != manifest.end())
        {
            std::stringstream ss(it->second);
            std::string suffix;
            while (std::getline(ss, suffix, ',')) { if (!suffix.empty()) { pack.missionSuffixes.push_back(suffix); } }
        }

        pack.strings = ParseKeyValueFile(folder / "strings.txt");
        return pack;
    }

    // Scans `languageFolder` for subfolders, loading each as a
    // LanguagePack - genuinely open-ended, not limited to a fixed list
    // (Edward, 2026: "ensure the system is modular and accepts new
    // languages"). Returns an empty list if the folder doesn't exist -
    // that's the expected state before any packs are installed, not an
    // error.
    inline std::vector<LanguagePack> DiscoverLanguagePacks(const std::filesystem::path& languageFolder)
    {
        std::vector<LanguagePack> packs;
        if (!std::filesystem::is_directory(languageFolder)) { return packs; }

        for (const auto& entry : std::filesystem::directory_iterator(languageFolder))
        {
            if (entry.is_directory()) { packs.push_back(LoadLanguagePack(entry.path())); }
        }
        return packs;
    }
}
