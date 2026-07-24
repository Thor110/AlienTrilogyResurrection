#pragma once

#include <string>

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
    // above - confirmed against the real file (uploaded as
    // "MISSIONE.TXT" for English): it's a SUFFIX, not a prefix, and
    // English gets an explicit letter too (unlike the AVI convention,
    // where English has none). "MISSION" (7 chars) + one letter = 8
    // chars exactly, so this never needs truncation either.
    inline std::string MissionTextFilename(Language language)
    {
        switch (language)
        {
        case Language::French:  return "MISSIONF";
        case Language::Italian: return "MISSIONI";
        case Language::Spanish: return "MISSIONS";
        case Language::English:
        default:                return "MISSIONE";
        }
    }
}
