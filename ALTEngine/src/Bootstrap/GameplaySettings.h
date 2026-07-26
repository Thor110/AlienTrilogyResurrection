#pragma once

#include "Config.h"
#include "Localization.h"

namespace ALTEngine::Bootstrap
{
    // Language wasn't persisted at all before this - main.cpp always
    // defaulted to Language::English on every boot regardless of what
    // was previously chosen in Options (Edward, 2026).
    class LanguageSettings
    {
    public:
        explicit LanguageSettings(Config& config) : config(config) {}

        Language Get() const
        {
            auto value = config.Get("Language");
            if (value == "French") { return Language::French; }
            if (value == "Italian") { return Language::Italian; }
            if (value == "Spanish") { return Language::Spanish; }
            return Language::English;
        }

        void Set(Language language)
        {
            const char* value = "English";
            switch (language)
            {
            case Language::French:  value = "French";  break;
            case Language::Italian: value = "Italian"; break;
            case Language::Spanish: value = "Spanish"; break;
            case Language::English:
            default: break;
            }
            config.Set("Language", value);
        }

    private:
        Config& config;
    };

    // "Acid Reign", "Raging Terror", "Xenomania" - the original game's
    // own difficulty names (Edward, 2026). Default is a guess (the
    // first/mildest-sounding one) rather than a confirmed "normal"
    // difficulty - correct this if the original defaults to a different
    // one.
    enum class Difficulty
    {
        AcidReign,
        RagingTerror,
        Xenomania,
    };

    class DifficultySettings
    {
    public:
        explicit DifficultySettings(Config& config) : config(config) {}

        Difficulty Get() const
        {
            auto value = config.Get("Difficulty");
            if (value == "RagingTerror") { return Difficulty::RagingTerror; }
            if (value == "Xenomania") { return Difficulty::Xenomania; }
            return Difficulty::AcidReign;
        }

        void Set(Difficulty difficulty)
        {
            const char* value = "AcidReign";
            if (difficulty == Difficulty::RagingTerror) { value = "RagingTerror"; }
            else if (difficulty == Difficulty::Xenomania) { value = "Xenomania"; }
            config.Set("Difficulty", value);
        }

    private:
        Config& config;
    };

    // Camera sway (head-bob style movement) - Off/On, matching the
    // Options menu's own two-item list. Default On is also a guess, not
    // confirmed against the original game's own default.
    class CameraSwaySettings
    {
    public:
        explicit CameraSwaySettings(Config& config) : config(config) {}

        bool Get() const
        {
            auto value = config.Get("CameraSway");
            return !value.has_value() || *value != "Off"; // default On
        }

        void Set(bool on)
        {
            config.Set("CameraSway", on ? "On" : "Off");
        }

    private:
        Config& config;
    };
}
