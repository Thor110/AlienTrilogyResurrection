#pragma once

#include "Config.h"

namespace ALTEngine::Bootstrap
{
    // Optional departures from the original game's behaviour. Every one
    // of these is off by default, so a fresh install behaves like the
    // 1996 release; the master toggle turns the whole set on or off
    // without disturbing the individual choices underneath it, so it can
    // be flipped back and forth without losing your preferences.
    //
    // Add new entries here and to MenuTree's Modern() list together.
    // Off  - everything original, whatever the individual toggles say
    // On   - every feature enabled, whatever the individual toggles say
    // Custom - each feature follows its own toggle
    enum class ModernMode
    {
        Off,
        Custom,
        On,
    };

    enum class ModernFeature
    {
        AutoOpenDoors, // doors open by walking onto their trigger cell rather than needing a deliberate use action
        KeepItems,     // carry weapons and ammo between levels instead of restarting each level with the pistol
    };

    class ModernSettings
    {
    public:
        explicit ModernSettings(Config& config) : config(config) {}

        ModernMode Mode() const
        {
            auto value = config.Get("ModernMode");
            if (!value.has_value()) { return ModernMode::Off; }
            if (*value == "On") { return ModernMode::On; }
            if (*value == "Custom") { return ModernMode::Custom; }
            return ModernMode::Off;
        }

        void SetMode(ModernMode mode)
        {
            const char* value = mode == ModernMode::On ? "On" : mode == ModernMode::Custom ? "Custom" : "Off";
            config.Set("ModernMode", value);
        }

        // The individual toggle's own stored value, ignoring the mode.
        // Menus want this so a feature still shows the state it will
        // return to under Custom.
        bool FeatureSetting(ModernFeature feature) const { return Read(KeyFor(feature), false); }
        void SetFeature(ModernFeature feature, bool enabled) { Write(KeyFor(feature), enabled); }

        // What the game should actually do.
        bool IsActive(ModernFeature feature) const
        {
            switch (Mode())
            {
            case ModernMode::On:     return true;
            case ModernMode::Custom: return FeatureSetting(feature);
            case ModernMode::Off:
            default:                 return false;
            }
        }

        static const char* KeyFor(ModernFeature feature)
        {
            switch (feature)
            {
            case ModernFeature::AutoOpenDoors: return "ModernAutoOpenDoors";
            case ModernFeature::KeepItems: return "ModernKeepItems";
            }
            return "ModernUnknown";
        }

    private:
        bool Read(const char* key, bool fallback) const
        {
            auto value = config.Get(key);
            if (!value.has_value()) { return fallback; }
            return *value == "On";
        }

        void Write(const char* key, bool enabled) { config.Set(key, enabled ? "On" : "Off"); }

        Config& config;
    };
}
