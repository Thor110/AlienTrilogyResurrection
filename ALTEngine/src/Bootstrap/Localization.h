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

    // English has no prefix. French is confirmed ('F', e.g. INTRO.AVI ->
    // FINTRO.AVI). Italian/Spanish are inferred as 'I'/'S' (matching
    // Italiano/Español) - NOT yet confirmed against real files the way
    // French is. Verify before relying on these two.
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
}
