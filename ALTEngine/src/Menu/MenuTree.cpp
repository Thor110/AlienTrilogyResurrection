#include "MenuTree.h"

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
                               bool includeCredits)
    {
        using ALTEngine::Bootstrap::StringId;
        return List("Main Menu", {
            Action("Start Game", StringId::StartGame),
            Action("Multiplayer", StringId::Multiplayer),
            Action("Load Game", StringId::LoadGame),
            Options(resolutionLabels, settings, keyBindings, audioSettings, includeCredits),
        });
    }
}
