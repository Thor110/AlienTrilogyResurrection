#pragma once

#include "../StringId.h"

#include <array>
#include <cstddef>

namespace ALTEngine::Bootstrap
{
    // Empty placeholder table (Edward, 2026: "set up empty placeholders
    // for the other languages currently in the list") - every entry
    // starts as nullptr ("not translated yet"), which Tr() falls back
    // to StringsEnglish() for. Fill entries in here as translations
    // become available; order must exactly match StringId.h, same as
    // every other language's table, so an automated translation pass
    // can fill this file in directly without touching any other code.
    //
    // The language-name entries are the one exception, pre-filled here
    // rather than left to fall back - a language conventionally names
    // itself the same way regardless of which language is currently
    // active (matches the existing Language submenu, which already
    // showed every option in its own language before this system
    // existed).
    inline const std::array<const char*, static_cast<std::size_t>(StringId::Count)>& StringsJapanese()
    {
        static const std::array<const char*, static_cast<std::size_t>(StringId::Count)> table = [] {
            std::array<const char*, static_cast<std::size_t>(StringId::Count)> t{};
            t[static_cast<std::size_t>(StringId::LanguageEnglish)] = "English";
            t[static_cast<std::size_t>(StringId::LanguageFrench)] = "Français";
            t[static_cast<std::size_t>(StringId::LanguageItalian)] = "Italiano";
            t[static_cast<std::size_t>(StringId::LanguageSpanish)] = "Español";
            t[static_cast<std::size_t>(StringId::LanguageGerman)] = "Deutsch";
            t[static_cast<std::size_t>(StringId::LanguageJapanese)] = "Japanese 日本語";
            return t;
        }();
        return table;
    }
}
