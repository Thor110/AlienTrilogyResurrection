#pragma once

#include <string>
#include <vector>

namespace ALTEngine::Bootstrap
{
    enum class Language
    {
        English,
        French,
        Italian,
        Spanish,
    };

    // English has no prefix. All four confirmed: French 'F' (INTRO.AVI ->
    // FINTRO.AVI), and English/French/Italian/Spanish 'E'/'F'/'I'/'S' via
    // the MISSION#.TXT files on disk.
    inline char LanguagePrefix(Language language)
    {
        switch (language)
        {
        case Language::French:  return 'F';
        case Language::Italian: return 'I';
        case Language::Spanish: return 'S';
        case Language::English:
        default:                return '\0';
        }
    }

    // Computes the on-disk base filename (no extension) for `baseName` in
    // `language`, following the DOS 8.3 filename rule that explains
    // FGAMEOVE.AVI: prepend the prefix, then truncate to 8 characters -
    // NOT a per-file exception. "INTRO" + 'F' -> "FINTRO" (6 chars, no
    // truncation needed). "GAMEOVER" + 'F' -> "FGAMEOVER" (9 chars) ->
    // truncated to "FGAMEOVE".
    inline std::string LocalizedBaseName(const std::string& baseName, Language language)
    {
        char prefix = LanguagePrefix(language);
        if (prefix == '\0') { return baseName; }

        std::string prefixed = std::string(1, prefix) + baseName;
        if (prefixed.size() > 8) { prefixed = prefixed.substr(0, 8); }
        return prefixed;
    }

    // MISSION#.TXT uses a DIFFERENT convention from LocalizedBaseName
    // above - confirmed against a real file (uploaded as "MISSIONE.TXT"):
    // it's a SUFFIX, not a prefix, and English gets an explicit letter
    // too (unlike the AVI convention, where English has none). "MISSION"
    // (7 chars) + one letter = 8 chars exactly, so this never needs
    // truncation either.
    //
    // For English specifically, there are TWO real variants depending on
    // the disc release: US copies ship MISSIONU.TXT, other English
    // releases ship MISSIONE.TXT (confirmed - Edward's test disc was a
    // US copy and only had MISSIONU.TXT, which silently produced an
    // empty briefing since only MISSIONE.TXT was ever checked). Returns
    // candidates in the order to try them - US first, since that's the
    // release this was actually confirmed against.
    inline std::vector<std::string> MissionTextFilenameCandidates(Language language)
    {
        switch (language)
        {
        case Language::French:  return { "MISSIONF" };
        case Language::Italian: return { "MISSIONI" };
        case Language::Spanish: return { "MISSIONS" };
        case Language::English:
        default:                return { "MISSIONU", "MISSIONE" };
        }
    }
}
