#include "MenuTree.h"
#include "../Bootstrap/ModernPresets.h"
#include "../Bootstrap/ModernSettings.h"

#include <string>
#include <utility>
#include <vector>

namespace ALTEngine::Menu
{
    namespace
    {
        // Thin aliases for MakeAction/MakeList (shared in MenuNode.h -
        // also used by PauseMenuTree.cpp now) - kept short here since
        // this file uses them constantly.
        MenuNode Action(std::string label, int modelIndex = -1) { return MakeAction(std::move(label), modelIndex); }
        MenuNode List(std::string label, std::vector<MenuNode> children, int modelIndex = -1) { return MakeList(std::move(label), std::move(children), modelIndex); }

        // Same, but also tags the node with a StringId for translated
        // display (Edward, 2026 - localization foundations). `label`
        // stays the plain English string either way - it's the stable
        // internal identifier every existing comparison already relies
        // on (ApplyLeafAction, FindChildIndexByLabel, etc), never the
        // translated text.
        MenuNode Action(std::string label, ALTEngine::Bootstrap::StringId id, int modelIndex = -1)
        {
            MenuNode n = MakeAction(std::move(label), modelIndex);
            n.stringId = static_cast<int>(id);
            return n;
        }
        MenuNode List(std::string label, ALTEngine::Bootstrap::StringId id, std::vector<MenuNode> children, int modelIndex = -1)
        {
            MenuNode n = MakeList(std::move(label), std::move(children), modelIndex);
            n.stringId = static_cast<int>(id);
            return n;
        }

        // Index of the child whose label matches `label`, or 0 (the old,
        // always-first-item default) if none match - used to compute
        // initialSelectedChild for settings lists (Edward, 2026).
        int FindChildIndexByLabel(const std::vector<MenuNode>& children, const std::string& label)
        {
            for (size_t i = 0; i < children.size(); i++)
            {
                if (children[i].label == label) { return static_cast<int>(i); }
            }
            return 0;
        }

        // Builds a device's full Redefine list - one Action per
        // AllActions() entry, labelled "{action name}: {current
        // binding}", with inputActionIndex/deviceIndex set so
        // MenuController knows which binding to capture when Enter is
        // pressed on it. One shared helper for every device rather than
        // a separate list-builder per device (Edward, 2026: "All
        // options in that list will eventually contain the same
        // options but only accept input from those specific devices...
        // turn the list into a helper that takes an input type, then
        // we can reuse it for keyboard/mouse and the other hardware
        // peripherals if I ever manage to get a hold of them").
        MenuNode BuildRedefineList(ALTEngine::Bootstrap::DeviceKind device, ALTEngine::Bootstrap::KeyBindings& keyBindings, ALTEngine::Bootstrap::Language language)
        {
            std::vector<MenuNode> children;
            for (auto action : ALTEngine::Bootstrap::AllActions())
            {
                MenuNode n = Action(keyBindings.FormatBinding(device, action, language));
                n.inputActionIndex = static_cast<int>(action);
                n.deviceIndex = static_cast<int>(device);
                children.push_back(std::move(n));
            }
            return List("Redefine", std::move(children));
        }

        // "Restore Defaults" -> "Are You Sure ?" -> No / Yes, below the
        // Redefine entry (Edward, 2026). "Yes" carries inputActionIndex
        // = -2, a sentinel distinct from -1 ("not a binding leaf") and
        // >=0 ("rebind this specific action") meaning "confirmed: reset
        // every action's binding for deviceIndex back to default" -
        // MenuController intercepts it the same way it already
        // intercepts individual rebind leaves. "No" is left as a plain,
        // unhandled Action, same as Exit Game's own "No" elsewhere.
        MenuNode BuildRestoreDefaultsList(ALTEngine::Bootstrap::DeviceKind device)
        {
            MenuNode yes = Action("Yes");
            yes.inputActionIndex = -2;
            yes.deviceIndex = static_cast<int>(device);
            return List("Restore Defaults", { Action("No"), std::move(yes) });
        }

        MenuNode Controls(ALTEngine::Bootstrap::KeyBindings& keyBindings, ALTEngine::Bootstrap::Language language)
        {
            using ALTEngine::Bootstrap::DeviceKind;
            using ALTEngine::Bootstrap::StringId;

            // Mouse Sensitivity (Edward, 2026) - rendered as an actual
            // slider bar (see Volume() for the same redesign and its
            // reasoning). inputActionIndex -5 is a new sentinel in the
            // same family as -3/-4 (volume) and -2 (restore defaults),
            // letting MenuController intercept left/right on this
            // specific entry.
            MenuNode mouseSensitivity = Action("Mouse Sensitivity", StringId::MouseSensitivity);
            mouseSensitivity.inputActionIndex = -5;
            mouseSensitivity.sliderValue = keyBindings.MouseSensitivity();

            MenuNode n = List("Controls", StringId::Controls, {
                // modelIndex on the List node itself (not the Redefine
                // child) - EffectiveModelIndex takes the deepest SET
                // index along the current path, so this shows the
                // correct model the moment Keyboard/Mouse is highlighted,
                // not only after pressing Enter into Redefine. Matches
                // the reference images, where highlighting alone changes
                // the background model (Edward, 2026).
                List("Keyboard", StringId::Keyboard, { BuildRedefineList(DeviceKind::Keyboard, keyBindings, language), BuildRestoreDefaultsList(DeviceKind::Keyboard) }, ModelIndex::Keyboard),
                List("Mouse", StringId::Mouse, { BuildRedefineList(DeviceKind::Mouse, keyBindings, language), BuildRestoreDefaultsList(DeviceKind::Mouse), std::move(mouseSensitivity) }, ModelIndex::Mouse),
                List("Joystick", StringId::Joystick, { Action("Joystick", StringId::Joystick) }, ModelIndex::Joystick),
                // Gravis Grip and Gravis Pad both use Multitap (index 3) -
                // Edward: both peripherals visually look like a multitap.
                // Real hardware to test against is going to be genuinely
                // hard to source either way, but the menu entries and
                // model association are ready regardless.
                Action("Gravis Grip", StringId::GravisGrip, ModelIndex::Multitap),
                Action("Gravis Pad", StringId::GravisPad, ModelIndex::Multitap),
                Action("SpaceOrb 360", StringId::SpaceOrb360, ModelIndex::Gamepad), // menu label differs from the OPTOBJ catalog's own generic name for this index
                Action("VFX-1", StringId::Vfx1, ModelIndex::Headphones),     // ditto - VFX-1 was a VR headset, repurposing "Headphones"
            });

            // Everything except Keyboard/Mouse - Edward, 2026: "All
            // options except Keyboard and Mouse in the controls list
            // should be disabled... I have no idea when I can hook up or
            // test that hardware." Indices 0/1 are Keyboard/Mouse.
            for (size_t i = 2; i < n.children.size(); i++) { n.children[i].enabled = false; }

            return n;
        }

        MenuNode Difficulty(ALTEngine::Bootstrap::Difficulty current)
        {
            using ALTEngine::Bootstrap::StringId;
            MenuNode n = List("Difficulty", StringId::Difficulty, {
                Action("Acid Reign", StringId::AcidReign),
                Action("Raging Terror", StringId::RagingTerror),
                Action("Xenomania", StringId::Xenomania),
            });
            const char* label = current == ALTEngine::Bootstrap::Difficulty::RagingTerror ? "Raging Terror"
                               : current == ALTEngine::Bootstrap::Difficulty::Xenomania ? "Xenomania"
                               : "Acid Reign";
            n.initialSelectedChild = FindChildIndexByLabel(n.children, label);
            n.isSettingsList = true;
            return n;
        }

        MenuNode CameraSway(bool currentlyOn)
        {
            using ALTEngine::Bootstrap::StringId;
            MenuNode n = List("Camera Sway", StringId::CameraSway, {
                Action("Off", StringId::Off, ModelIndex::CameraCrossedOut),
                Action("On", StringId::On, ModelIndex::Camera),
            });
            n.initialSelectedChild = FindChildIndexByLabel(n.children, currentlyOn ? "On" : "Off");
            n.isSettingsList = true;
            return n;
        }

        MenuNode Graphics(const std::vector<std::string>& resolutionLabels, ALTEngine::Bootstrap::RenderFidelity currentQuality,
                          const std::string& currentResolutionLabel, bool currentVSync, ALTEngine::Bootstrap::DisplayMode currentDisplayMode)
        {
            using ALTEngine::Bootstrap::StringId;
            using ALTEngine::Bootstrap::DisplayMode;
            // New - not in the original game. "Quality" ties to
            // RenderSettings; "Resolution" ties to ResolutionSettings.
            // resolutionLabels' own children ("1920x1080" etc) are
            // dynamic content, not fixed UI strings - left untagged
            // (stringId stays -1, showing the label as-is).
            std::vector<MenuNode> resolutionChildren;
            for (const auto& label : resolutionLabels) { resolutionChildren.push_back(Action(label)); }

            MenuNode quality = List("Quality", StringId::Quality, {
                Action("Original", StringId::Original),
                Action("Smoothed", StringId::Smoothed),
            });
            quality.initialSelectedChild = FindChildIndexByLabel(quality.children,
                currentQuality == ALTEngine::Bootstrap::RenderFidelity::Smoothed ? "Smoothed" : "Original");
            quality.isSettingsList = true;

            MenuNode resolution = List("Resolution", StringId::Resolution, std::move(resolutionChildren));
            resolution.initialSelectedChild = FindChildIndexByLabel(resolution.children, currentResolutionLabel);
            resolution.isSettingsList = true;

            // VSync and Display Mode (Edward, 2026) - same "settings
            // list" pattern as Quality/Resolution above.
            MenuNode vsync = List("VSync", StringId::VSync, {
                Action("Off", StringId::Off),
                Action("On", StringId::On),
            });
            vsync.initialSelectedChild = FindChildIndexByLabel(vsync.children, currentVSync ? "On" : "Off");
            vsync.isSettingsList = true;

            MenuNode displayMode = List("Display Mode", StringId::DisplayModeTitle, {
                Action("Windowed", StringId::Windowed),
                Action("Fullscreen", StringId::Fullscreen),
                Action("Borderless", StringId::Borderless),
            });
            const char* displayModeLabel = currentDisplayMode == DisplayMode::Windowed ? "Windowed"
                                          : currentDisplayMode == DisplayMode::Borderless ? "Borderless"
                                          : "Fullscreen";
            displayMode.initialSelectedChild = FindChildIndexByLabel(displayMode.children, displayModeLabel);
            displayMode.isSettingsList = true;

            return List("Graphics", StringId::Graphics, { std::move(quality), std::move(resolution), std::move(vsync), std::move(displayMode) });
        }

        MenuNode LanguageMenu(ALTEngine::Bootstrap::Language current)
        {
            using ALTEngine::Bootstrap::StringId;
            MenuNode n = List("Language", StringId::LanguageMenuTitle, {
                Action("English", StringId::LanguageEnglish),          // English
                Action("Français", StringId::LanguageFrench),         // French
                Action("Italiano", StringId::LanguageItalian),         // Italian
                Action("Español", StringId::LanguageSpanish),          // Spanish
                Action("Deutsch", StringId::LanguageGerman),          // German
                Action("Japanese 日本語", StringId::LanguageJapanese),   // Japanese
            });
            const char* label = "English";
            switch (current)
            {
            case ALTEngine::Bootstrap::Language::French:  label = "Français"; break;
            case ALTEngine::Bootstrap::Language::Italian: label = "Italiano"; break;
            case ALTEngine::Bootstrap::Language::Spanish: label = "Español";  break;
            case ALTEngine::Bootstrap::Language::German:  label = "Deutsch";  break;
            case ALTEngine::Bootstrap::Language::Japanese: label = "Japanese 日本語"; break;
            case ALTEngine::Bootstrap::Language::English:
            default: break;
            }
            n.initialSelectedChild = FindChildIndexByLabel(n.children, label);
            n.isSettingsList = true;
            return n;
        }

        MenuNode Volume(ALTEngine::Bootstrap::AudioSettings& audioSettings, ALTEngine::Bootstrap::Language)
        {
            using ALTEngine::Bootstrap::StringId;
            /*
            MenuNode n;
            n.label = "Volume";
            n.kind = MenuNodeKind::Slider;
            return n;
            */ // TODO : Black is transparent on these textures. RGB 0/0/0 HSL 160/0/0
            // only appears to be on these two models / textures.
            //
            // Rendered as an actual slider bar, not a text readout
            // (Edward, 2026: "keep the design of the slider which now
            // only resides in the pause menu... revert to that so that
            // we can match the original aesthetic") - label stays the
            // plain translated name (stringId set normally), the current
            // value lives in sliderValue and MenuController's DrawColumn
            // renders it via the shared DrawSlider. inputActionIndex
            // borrows the same sentinel mechanism Redefine/Restore
            // Defaults already use (-3/-4) so MenuController's input
            // handling knows to intercept left/right on these entries.
            MenuNode music = Action("Music", StringId::Music, ModelIndex::SpeakerMusic);
            music.inputActionIndex = -3;
            music.sliderValue = audioSettings.MusicVolume();
            MenuNode sfx = Action("SFX", StringId::Sfx, ModelIndex::SpeakerSfx);
            sfx.inputActionIndex = -4;
            sfx.sliderValue = audioSettings.SfxVolume();

            return List("Volume", StringId::Volume, { std::move(music), std::move(sfx) });
        }

        MenuNode Credits()
        {
            MenuNode n;
            n.label = "Credits";
            n.stringId = static_cast<int>(ALTEngine::Bootstrap::StringId::Credits);
            n.kind = MenuNodeKind::CreditsScroll;
            return n;
        }

        // Optional departures from original behaviour, all off by
        // default. Reads its current values directly rather than through
        // MenuSettingsSnapshot - worth folding into the snapshot if this
        // list grows.
        MenuNode Modern()
        {
            using ALTEngine::Bootstrap::StringId;
            ALTEngine::Bootstrap::Config config;
            ALTEngine::Bootstrap::ModernSettings modern(config);

            // Off / Custom / On, then one row per preset. Presets come from
            // the single table in ModernPresets.h, so adding one needs no change
            // here - and each row's displayed name and description come from its
            // StringIds, so they translate like everything else.
            std::vector<MenuNode> modeRows = {
                Action("Off", StringId::Off),
                Action("Custom", StringId::Custom),
                Action("On", StringId::On),
            };
            for (const auto& preset : ALTEngine::Bootstrap::PRESETS)
            {
                // The internal label is the preset KEY, which is what the
                // controller matches on and what gets persisted; the visible
                // text is the translated name.
                MenuNode row = Action(std::string(preset.key), preset.nameId);
                row.descriptionStringId = static_cast<int>(preset.descriptionId);
                modeRows.push_back(std::move(row));
            }

            MenuNode enableAll = List("Enable All", StringId::EnableAll, std::move(modeRows));
            auto mode = modern.Mode();
            enableAll.initialSelectedChild = FindChildIndexByLabel(
                enableAll.children,
                mode == ALTEngine::Bootstrap::ModernMode::On ? "On"
                    : mode == ALTEngine::Bootstrap::ModernMode::Custom ? "Custom"
                    : mode == ALTEngine::Bootstrap::ModernMode::Preset ? modern.PresetKey().c_str()
                    : "Off");
            enableAll.isSettingsList = true;

            MenuNode autoDoors = List("Automatic Doors", StringId::AutomaticDoors, {
                Action("Off", StringId::Off),
                Action("On", StringId::On),
            });
            // Show what the feature will actually do, not just its stored
            // toggle - under On or Off the mode overrides the toggle, and
            // displaying the stale stored value reads as the setting having
            // been ignored. The stored value is still what Custom returns to.
            bool autoDoorsEffective =
                modern.IsActive(ALTEngine::Bootstrap::ModernFeature::AutoOpenDoors);
            autoDoors.initialSelectedChild = FindChildIndexByLabel(autoDoors.children, autoDoorsEffective ? "On" : "Off");
            autoDoors.isSettingsList = true;

            MenuNode keepItems = List("Keep Items", StringId::KeepItems, {
                Action("Off", StringId::Off),
                Action("On", StringId::On),
            });
            bool keepItemsEffective =
                mode == ALTEngine::Bootstrap::ModernMode::On ? true
                : mode == ALTEngine::Bootstrap::ModernMode::Off ? false
                : modern.FeatureSetting(ALTEngine::Bootstrap::ModernFeature::KeepItems);
            keepItems.initialSelectedChild = FindChildIndexByLabel(keepItems.children, keepItemsEffective ? "On" : "Off");
            keepItems.isSettingsList = true;

            auto featureNode = [&](const char* label, StringId title, StringId description,
                                   ALTEngine::Bootstrap::ModernFeature feature) {
                MenuNode node = List(label, title, {
                    Action("Off", StringId::Off),
                    Action("On", StringId::On),
                });
                // Read through IsActive so Preset mode displays what the preset
                // actually does, not the stale stored toggle underneath it.
                bool effective = modern.IsActive(feature);
                node.initialSelectedChild = FindChildIndexByLabel(node.children, effective ? "On" : "Off");
                node.isSettingsList = true;
                node.descriptionStringId = static_cast<int>(description);
                return node;
            };

            enableAll.descriptionStringId = static_cast<int>(StringId::DescEnableAll);
            autoDoors.descriptionStringId = static_cast<int>(StringId::DescAutomaticDoors);
            keepItems.descriptionStringId = static_cast<int>(StringId::DescKeepItems);

            std::vector<MenuNode> entries = {
                std::move(enableAll),
                std::move(autoDoors),
                std::move(keepItems),
                featureNode(ALTEngine::Bootstrap::ModernSettings::MenuLabel(ALTEngine::Bootstrap::ModernFeature::SkipEndLevelScreen),
                            StringId::SkipEndLevelScreen, StringId::DescSkipEndLevel,
                            ALTEngine::Bootstrap::ModernFeature::SkipEndLevelScreen),
                featureNode(ALTEngine::Bootstrap::ModernSettings::MenuLabel(ALTEngine::Bootstrap::ModernFeature::PlayerJumping),
                            StringId::PlayerJumping, StringId::DescPlayerJumping,
                            ALTEngine::Bootstrap::ModernFeature::PlayerJumping),
                featureNode(ALTEngine::Bootstrap::ModernSettings::MenuLabel(ALTEngine::Bootstrap::ModernFeature::StunnedEnemies),
                            StringId::StunnedEnemies, StringId::DescStunnedEnemies,
                            ALTEngine::Bootstrap::ModernFeature::StunnedEnemies),
                featureNode(ALTEngine::Bootstrap::ModernSettings::MenuLabel(ALTEngine::Bootstrap::ModernFeature::FreeLook),
                            StringId::FreeLook, StringId::DescFreeLook,
                            ALTEngine::Bootstrap::ModernFeature::FreeLook),
                featureNode(ALTEngine::Bootstrap::ModernSettings::MenuLabel(ALTEngine::Bootstrap::ModernFeature::RenderDistance),
                            StringId::RenderDistance, StringId::DescRenderDistance,
                            ALTEngine::Bootstrap::ModernFeature::RenderDistance),
                featureNode(ALTEngine::Bootstrap::ModernSettings::MenuLabel(ALTEngine::Bootstrap::ModernFeature::LevelSelect),
                            StringId::LevelSelect, StringId::DescLevelSelect,
                            ALTEngine::Bootstrap::ModernFeature::LevelSelect),
                featureNode(ALTEngine::Bootstrap::ModernSettings::MenuLabel(ALTEngine::Bootstrap::ModernFeature::LiveMinimap),
                            StringId::LiveMinimap, StringId::DescLiveMinimap,
                            ALTEngine::Bootstrap::ModernFeature::LiveMinimap),
                featureNode(ALTEngine::Bootstrap::ModernSettings::MenuLabel(ALTEngine::Bootstrap::ModernFeature::EnableCheats),
                            StringId::EnableCheats, StringId::DescEnableCheats,
                            ALTEngine::Bootstrap::ModernFeature::EnableCheats),
            };

            return List("Modern", StringId::Modern, std::move(entries));
        }

        MenuNode Options(const std::vector<std::string>& resolutionLabels, const MenuSettingsSnapshot& settings,
                         ALTEngine::Bootstrap::KeyBindings& keyBindings, ALTEngine::Bootstrap::AudioSettings& audioSettings,
                         bool includeCredits)
        {
            using ALTEngine::Bootstrap::StringId;
            // modelIndex Computer here is inherited by every child that
            // doesn't set its own - matches the reference images, where
            // the monitor+tower model is the default across almost all of
            // Options and only changes for Controls > Keyboard > Redefine.
            std::vector<MenuNode> children = {
                Volume(audioSettings, settings.language),
                Controls(keyBindings, settings.language),
                Difficulty(settings.difficulty),
                CameraSway(settings.cameraSwayOn),
                Graphics(resolutionLabels, settings.quality, settings.resolutionLabel, settings.vsync, settings.displayMode),
                Modern(),
                LanguageMenu(settings.language),
            };
            // Excluded when opened from the pause menu - Credits doesn't
            // make sense mid-game (Edward, 2026: "when coming from the
            // pause menu, the Credits button does not spawn").
            if (includeCredits) { children.push_back(Credits()); }

            return List("Options", StringId::Options, std::move(children), ModelIndex::Computer);
        }
    }

    MenuNode BuildMainMenuTree(const std::vector<std::string>& resolutionLabels, const MenuSettingsSnapshot& settings,
                               ALTEngine::Bootstrap::KeyBindings& keyBindings, ALTEngine::Bootstrap::AudioSettings& audioSettings,
                               bool includeCredits,
                               const std::vector<std::string>& levelSelectLabels)
    {
        using ALTEngine::Bootstrap::StringId;

        // Level Select, above Start Game, when the Modern feature is on - the
        // caller passes rows only in that case. Rows carry NO StringId: a level
        // code is not translatable, and stringId = -1 is what makes
        // DisplayLabel show the label verbatim. StringId::Count is one PAST the
        // end of the table and crashes on open.
        std::vector<MenuNode> top;
        if (!levelSelectLabels.empty())
        {
            std::vector<MenuNode> rows;
            for (const std::string& label : levelSelectLabels)
            {
                rows.push_back(Action(label));
            }
            // Internal label is deliberately NOT "Level Select": the Modern
            // feature toggle already uses that, and two nodes sharing a label
            // made the row handler fire when the TOGGLE was set to On - it saw
            // parent "Level Select" with leaf "On" and passed "On" along as a
            // level code (Edward, 2026). `label` is the internal identifier;
            // the StringId is what gets displayed, so this is invisible.
            top.push_back(List(LEVEL_SELECT_LIST_LABEL, StringId::LevelSelect, std::move(rows)));
        }
        top.push_back(Action("Start Game", StringId::StartGame));
        top.push_back(Action("Multiplayer", StringId::Multiplayer));
        top.push_back(Action("Load Game", StringId::LoadGame));
        top.push_back(Options(resolutionLabels, settings, keyBindings, audioSettings, includeCredits));
        return List("Main Menu", top);
    }
}
