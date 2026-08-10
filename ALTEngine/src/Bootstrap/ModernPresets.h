#pragma once

#include "ModernSettings.h"
#include "StringId.h"

#include <array>
#include <string_view>

namespace ALTEngine::Bootstrap
{
    // Named bundles of Modern feature states, so a player can pick a whole
    // playstyle instead of setting seven toggles.
    //
    // TO ADD A PRESET, three edits and nothing else:
    //   1. a StringId for its name and one for its description (StringId.h,
    //      StringKeyName.h, Strings_English.h - the other languages fall back
    //      to English until translated, so they need no change),
    //   2. an entry in PRESETS below listing only the features it turns ON,
    //   3. nothing. The menu builds its own row, the mode handling picks it up,
    //      and it persists by `key`.
    //
    // `key` is what goes in the config file, so it must never change once
    // shipped or saved settings would silently fall back. The displayed name
    // comes from `nameId` through Tr(), so presets are translatable like every
    // other menu string.
    struct ModernPreset
    {
        std::string_view key;
        StringId nameId;
        StringId descriptionId;

        // Features this preset enables. Anything not listed is off. Sized to
        // the feature count so a preset can name all of them.
        std::array<ModernFeature, 8> enabled;
        int enabledCount;

        bool Enables(ModernFeature feature) const
        {
            for (int i = 0; i < enabledCount; ++i)
            {
                if (enabled[i] == feature) { return true; }
            }
            return false;
        }
    };

    inline constexpr ModernPreset PRESETS[] = {
        // Quality-of-life only: nothing that changes how the game plays or what
        // the player can see, just the things that remove friction.
        {
            "Convenience",
            StringId::PresetConvenience,
            StringId::DescPresetConvenience,
            { ModernFeature::AutoOpenDoors,
              ModernFeature::KeepItems,
              ModernFeature::SkipEndLevelScreen },
            3
        },
        // How a modern shooter would present it: free mouse look and no
        // draw-distance fade, on top of the convenience features.
        {
            "Modernised",
            StringId::PresetModernised,
            StringId::DescPresetModernised,
            { ModernFeature::AutoOpenDoors,
              ModernFeature::KeepItems,
              ModernFeature::SkipEndLevelScreen,
              ModernFeature::FreeLook,
              ModernFeature::RenderDistance },
            5
        },
        // For working on the port rather than playing it.
        {
            "Testing",
            StringId::PresetTesting,
            StringId::DescPresetTesting,
            { ModernFeature::LevelSelect,
              ModernFeature::LiveMinimap,
              ModernFeature::FreeLook,
              ModernFeature::RenderDistance },
            4
        },
    };

    inline constexpr int PRESET_COUNT = static_cast<int>(sizeof(PRESETS) / sizeof(PRESETS[0]));

    // Resolves the selected preset. An unknown or unset key resolves to
    // nothing enabled, i.e. the same as Off - the safe direction, since it means
    // a config naming a preset this build does not have behaves like the
    // original game rather than turning things on at random.
    //
    // Defined here rather than in ModernSettings.h because the preset table
    // needs ModernFeature, and ModernSettings must not depend on the table.
    inline const ModernPreset* FindPreset(std::string_view key)
    {
        for (const ModernPreset& preset : PRESETS)
        {
            if (preset.key == key) { return &preset; }
        }
        return nullptr;
    }

    inline const ModernPreset* FindPresetOrNull(std::string_view key) { return FindPreset(key); }
}

namespace ALTEngine::Bootstrap
{
    inline bool ModernSettings::PresetEnables(ModernFeature feature) const
    {
        const ModernPreset* preset = FindPreset(PresetKey());
        return preset ? preset->Enables(feature) : false;
    }
}
