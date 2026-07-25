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

    // MISSION#.TXT-style suffix letters - confirmed against real
    // MISSIONE.TXT / MISSIONU.TXT, and also needed for PNL0GFX#.16/
    // PNL1GFX#.16. The FULL set that actually exists on disc, confirmed
    // directly from TRILOGY.EXE's embedded file table (Edward, 2026):
    // E/J/U/G/I/S/F (English/Japanese/US/German/Italian/Spanish/French)
    // - two more than "U/E/F/I/S" originally suggested. German and
    // Japanese aren't first-class Language values in this engine yet
    // (would touch the boot menu's language submenu and several other
    // spots to add properly), so they're not listed as candidates below
    // - if that support gets added later, this is where their letters
    // ('G', 'J') go. Returns candidates in the order to try them - for
    // English, US first (that's the release the English-file gap was
    // actually confirmed against), then the other English variant.
    inline std::vector<char> LanguageSuffixCandidates(Language language)
    {
        switch (language)
        {
        case Language::French:  return { 'F' };
        case Language::Italian: return { 'I' };
        case Language::Spanish: return { 'S' };
        case Language::English:
        default:                return { 'U', 'E' };
        }
    }

    // MISSION#.TXT uses a DIFFERENT convention from LocalizedBaseName
    // above - confirmed against a real file (uploaded as "MISSIONE.TXT"):
    // it's a SUFFIX, not a prefix, and English gets an explicit letter
    // too (unlike the AVI convention, where English has none). "MISSION"
    // (7 chars) + one letter = 8 chars exactly, so this never needs
    // truncation either.
    inline std::vector<std::string> MissionTextFilenameCandidates(Language language)
    {
        std::vector<std::string> result;
        for (char c : LanguageSuffixCandidates(language)) { result.push_back(std::string("MISSION") + c); }
        return result;
    }
}
