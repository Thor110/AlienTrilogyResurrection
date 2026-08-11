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

        // A named bundle of feature states - see ModernPresets.h. Which one is
        // stored separately, under "ModernPreset", so the mode and the choice
        // do not have to share one config value.
        Preset,
    };

    enum class ModernFeature
    {
        AutoOpenDoors, // doors open by walking onto their trigger cell rather than needing a deliberate use action
        KeepItems,          // carry weapons and ammo between levels instead of restarting each level with the pistol
        SkipEndLevelScreen, // go straight on rather than showing the end-of-level prompt
        PlayerJumping,      // enable a jump action, and the extra control binding it needs
        StunnedEnemies,     // disable enemy stun-lock so they can be damaged repeatedly

        // Mouse pitch control. The original had NO free look: the camera's
        // pitch was driven entirely by the floor, panning up or down as the
        // player walked stairs and ramps. Turning this off restores that, and
        // is the reason the -GAP override geometry exists at all - with the
        // original camera you can never see the holes above door frames,
        // because you can never look up.
        FreeLook,

        // Unlimited draw distance. The original faded everything beyond a
        // fixed range to black - not a cosmetic haze but the actual limit of
        // what it drew, which is why its corridors read as pitch dark a couple
        // of rooms away. Off restores that fade; On shows the whole level.
        RenderDistance,

        // Adds a Level Select entry to the main menu, letting any level be
        // started directly. The original had no such thing - levels only
        // progressed - so it is a modern convenience like the rest of these.
        LevelSelect,

        // Draws the pause menu's map in the corner of the screen during play.
        // The original only ever showed a map when paused, so having it live is
        // a departure and belongs here.
        LiveMinimap,

        // Adds a Cheats list to the pause menu. Off by default, and the cheats
        // themselves are inert until it is on - so a normal game cannot reach
        // them by accident.
        EnableCheats,
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
            if (*value == "Preset") { return ModernMode::Preset; }
            return ModernMode::Off;
        }

        void SetMode(ModernMode mode)
        {
            const char* value = mode == ModernMode::On ? "On"
                              : mode == ModernMode::Custom ? "Custom"
                              : mode == ModernMode::Preset ? "Preset" : "Off";
            config.Set("ModernMode", value);
        }

        // Every feature, for iteration. Must list all of them.
        static constexpr ModernFeature AllFeatures[] = {
            ModernFeature::AutoOpenDoors,
            ModernFeature::KeepItems,
            ModernFeature::SkipEndLevelScreen,
            ModernFeature::PlayerJumping,
            ModernFeature::StunnedEnemies,
            ModernFeature::FreeLook,
            ModernFeature::RenderDistance,
            ModernFeature::LevelSelect,
            ModernFeature::LiveMinimap,
            ModernFeature::EnableCheats,
        };

        // The individual toggle's own stored value, ignoring the mode.
        // Menus want this so a feature still shows the state it will
        // return to under Custom.
        bool FeatureSetting(ModernFeature feature) const { return Read(KeyFor(feature), false); }

        // Sets one feature and switches to Custom, which is the only mode in
        // which an individual toggle means anything.
        //
        // Crucially it first writes every OTHER feature's currently EFFECTIVE
        // value into storage. Without that step, changing one thing silently
        // reset everything else: under On or Off the individual stored values
        // are ignored, and they default to false, so the moment the mode
        // flipped to Custom every feature the user had not explicitly touched
        // read back as off. From the outside that looked like the individual
        // toggles not working at all and only the global On/Off having any
        // effect (Edward, 2026).
        void SetFeature(ModernFeature feature, bool enabled)
        {
            if (Mode() != ModernMode::Custom)
            {
                // Materialise each other feature's currently EFFECTIVE state,
                // read through IsActive rather than derived from the mode.
                //
                // This used to be `bool effective = (Mode() == On)`, which was
                // right for On and Off but wrong for a Preset: leaving a preset
                // by turning one feature off wrote false over every other
                // feature, so the whole preset collapsed instead of becoming an
                // editable copy of itself.
                bool effective[sizeof(AllFeatures) / sizeof(AllFeatures[0])]{};
                size_t index = 0;
                for (ModernFeature other : AllFeatures) { effective[index++] = IsActive(other); }

                index = 0;
                for (ModernFeature other : AllFeatures)
                {
                    bool value = effective[index++];
                    if (other == feature) { continue; }
                    Write(KeyFor(other), value);
                }
            }
            Write(KeyFor(feature), enabled);
            if (Mode() != ModernMode::Custom) { SetMode(ModernMode::Custom); }
        }

        // What the game should actually do.
        // Which preset is selected. Empty when none has ever been chosen.
        std::string PresetKey() const
        {
            auto value = config.Get("ModernPreset");
            return value.has_value() ? *value : std::string();
        }

        // Selects a preset AND switches to Preset mode, since choosing one
        // without switching would store a choice that does nothing.
        void SetPreset(const std::string& key)
        {
            config.Set("ModernPreset", key);
            SetMode(ModernMode::Preset);
        }

        // Defined out of line in ModernPresets.h's translation-unit-free way -
        // see the note there. Declared here so IsActive can use it.
        bool PresetEnables(ModernFeature feature) const;

        bool IsActive(ModernFeature feature) const
        {
            switch (Mode())
            {
            case ModernMode::On:     return true;
            case ModernMode::Custom: return FeatureSetting(feature);
            case ModernMode::Preset: return PresetEnables(feature);
            case ModernMode::Off:
            default:                 return false;
            }
        }

        // The menu label for a feature. Lives next to the feature itself so
        // MenuTree and MenuController cannot disagree about it - the menu node
        // is built from this and every label comparison resolves through it.
        static const char* MenuLabel(ModernFeature feature)
        {
            switch (feature)
            {
            case ModernFeature::AutoOpenDoors: return "Automatic Doors";
            case ModernFeature::KeepItems: return "Keep Items";
            case ModernFeature::SkipEndLevelScreen: return "Skip End Level Screen";
            case ModernFeature::PlayerJumping: return "Player Jumping";
            case ModernFeature::StunnedEnemies: return "Stunned Enemies";
            case ModernFeature::FreeLook: return "Free Look";
            case ModernFeature::RenderDistance: return "Render Distance";
            case ModernFeature::LevelSelect: return "Level Select";
            case ModernFeature::LiveMinimap: return "Live Minimap";
            case ModernFeature::EnableCheats: return "Enable Cheats";
            }
            return "";
        }

        static const char* KeyFor(ModernFeature feature)
        {
            switch (feature)
            {
            case ModernFeature::AutoOpenDoors: return "ModernAutoOpenDoors";
            case ModernFeature::KeepItems: return "ModernKeepItems";
            case ModernFeature::SkipEndLevelScreen: return "ModernSkipEndLevelScreen";
            case ModernFeature::PlayerJumping: return "ModernPlayerJumping";
            case ModernFeature::StunnedEnemies: return "ModernStunnedEnemies";
            case ModernFeature::FreeLook: return "ModernFreeLook";
            case ModernFeature::RenderDistance: return "ModernRenderDistance";
            case ModernFeature::LevelSelect: return "ModernLevelSelect";
            case ModernFeature::LiveMinimap: return "ModernLiveMinimap";
            case ModernFeature::EnableCheats: return "ModernEnableCheats";
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
