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

        MenuNode Controls()
        {
            MenuNode n = List("Controls", {
                // modelIndex on the List node itself (not the Redefine
                // child) - EffectiveModelIndex takes the deepest SET
                // index along the current path, so this shows the
                // correct model the moment Keyboard/Mouse is highlighted,
                // not only after pressing Enter into Redefine. Matches
                // the reference images, where highlighting alone changes
                // the background model (Edward, 2026).
                List("Keyboard", { Action("Redefine") }, ModelIndex::Keyboard),
                List("Mouse", { Action("Redefine") }, ModelIndex::Mouse),
                List("Joystick", { Action("Joystick") }, ModelIndex::Joystick),
                // Gravis Grip and Gravis Pad both use Multitap (index 3) -
                // Edward: both peripherals visually look like a multitap.
                // Real hardware to test against is going to be genuinely
                // hard to source either way, but the menu entries and
                // model association are ready regardless.
                Action("Gravis Grip", ModelIndex::Multitap),
                Action("Gravis Pad", ModelIndex::Multitap),
                Action("SpaceOrb 360", ModelIndex::Gamepad), // menu label differs from the OPTOBJ catalog's own generic name for this index
                Action("VFX-1", ModelIndex::Headphones),     // ditto - VFX-1 was a VR headset, repurposing "Headphones"
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
            MenuNode n = List("Difficulty", {
                Action("Acid Reign"),
                Action("Raging Terror"),
                Action("Xenomania"),
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
            MenuNode n = List("Camera Sway", {
                Action("Off", ModelIndex::CameraCrossedOut),
                Action("On", ModelIndex::Camera),
            });
            n.initialSelectedChild = FindChildIndexByLabel(n.children, currentlyOn ? "On" : "Off");
            n.isSettingsList = true;
            return n;
        }

        MenuNode Graphics(const std::vector<std::string>& resolutionLabels, ALTEngine::Bootstrap::RenderFidelity currentQuality,
                          const std::string& currentResolutionLabel)
        {
            // New - not in the original game. "Quality" ties to
            // RenderSettings; "Resolution" ties to ResolutionSettings.
            std::vector<MenuNode> resolutionChildren;
            for (const auto& label : resolutionLabels) { resolutionChildren.push_back(Action(label)); }

            MenuNode quality = List("Quality", {
                Action("Original"),
                Action("Smoothed"),
            });
            quality.initialSelectedChild = FindChildIndexByLabel(quality.children,
                currentQuality == ALTEngine::Bootstrap::RenderFidelity::Smoothed ? "Smoothed" : "Original");
            quality.isSettingsList = true;

            MenuNode resolution = List("Resolution", std::move(resolutionChildren));
            resolution.initialSelectedChild = FindChildIndexByLabel(resolution.children, currentResolutionLabel);
            resolution.isSettingsList = true;

            return List("Graphics", { std::move(quality), std::move(resolution) });
        }

        MenuNode LanguageMenu(ALTEngine::Bootstrap::Language current)
        {
            MenuNode n = List("Language", {
                Action("English"),          // English
                Action("Français"),         // French
                Action("Italiano"),         // Italian
                Action("Español"),          // Spanish
                Action("Deutsch"),          // German
                Action("Japanese 日本語"),   // Japanese
            });
            const char* label = "English";
            switch (current)
            {
            case ALTEngine::Bootstrap::Language::French:  label = "Français"; break;
            case ALTEngine::Bootstrap::Language::Italian: label = "Italiano"; break;
            case ALTEngine::Bootstrap::Language::Spanish: label = "Español";  break;
            case ALTEngine::Bootstrap::Language::English:
            default: break;
            }
            n.initialSelectedChild = FindChildIndexByLabel(n.children, label);
            n.isSettingsList = true;
            return n;
        }

        MenuNode Volume()
        {
            /*
            MenuNode n;
            n.label = "Volume";
            n.kind = MenuNodeKind::Slider;
            return n;
            */ // TODO : Black is transparent on these textures. RGB 0/0/0 HSL 160/0/0
            // only appears to be on these two models / textures.
            return List("Volume", {
                Action("Music", ModelIndex::SpeakerMusic),
                Action("SFX", ModelIndex::SpeakerSfx),
            });
        }

        MenuNode Credits()
        {
            MenuNode n;
            n.label = "Credits";
            n.kind = MenuNodeKind::CreditsScroll;
            return n;
        }

        MenuNode Options(const std::vector<std::string>& resolutionLabels, const MenuSettingsSnapshot& settings)
        {
            // modelIndex Computer here is inherited by every child that
            // doesn't set its own - matches the reference images, where
            // the monitor+tower model is the default across almost all of
            // Options and only changes for Controls > Keyboard > Redefine.
            return List("Options", {
                Volume(),
                Controls(),
                Difficulty(settings.difficulty),
                CameraSway(settings.cameraSwayOn),
                Graphics(resolutionLabels, settings.quality, settings.resolutionLabel),
                LanguageMenu(settings.language),
                Credits(),
            }, ModelIndex::Computer);
        }
    }

    MenuNode BuildMainMenuTree(const std::vector<std::string>& resolutionLabels, const MenuSettingsSnapshot& settings)
    {
        return List("Main Menu", {
            Action("Start Game"),
            Action("Multiplayer"),
            Action("Load Game"),
            Options(resolutionLabels, settings),
        });
    }
}
