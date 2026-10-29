#pragma once

#include "Config.h"
#include "Localization.h"

#include <algorithm>
#include <string>

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

    // How long one of the original's drawn frames lasts, in its timer
    // interrupts (69.74 Hz), for the weapon. The original fires and steps the
    // weapon's animation once per drawn frame (FUN_0003d5b0 and FUN_0003e93c
    // run only on the last pass, 0x6164 == 1). Its speed compensation
    // (FUN_00012590) adds passes for the player and the creatures on a slow
    // frame, but not for the weapon, so the original's rate of fire followed
    // the frame rate of the machine. The port runs the weapon on a fixed
    // clock of this many interrupts instead, so it is the same at any frame
    // rate. 2 is the original's design point (35 fps); the default, 3, is
    // about 23 fps. Stored as "WeaponFrameInterrupts", 1 to 8.
    class WeaponPacingSettings
    {
    public:
        static constexpr int DEFAULT = 3;
        static constexpr int MINIMUM = 1;
        static constexpr int MAXIMUM = 8;

        explicit WeaponPacingSettings(Config& config) : config(config) {}

        int Get() const
        {
            auto value = config.Get("WeaponFrameInterrupts");
            if (!value) { return DEFAULT; }
            try { return std::clamp(std::stoi(*value), MINIMUM, MAXIMUM); }
            catch (...) { return DEFAULT; }
        }

        void Set(int interrupts)
        {
            config.Set("WeaponFrameInterrupts", std::to_string(std::clamp(interrupts, MINIMUM, MAXIMUM)));
        }

    private:
        Config& config;
    };
}
