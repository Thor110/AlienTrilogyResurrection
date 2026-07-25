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

        MenuNode Controls()
        {
            return List("Controls", {
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
        }

        MenuNode Difficulty()
        {
            return List("Difficulty", {
                Action("Acid Reign"),
                Action("Raging Terror"),
                Action("Xenomania"),
            });
        }

        MenuNode CameraSway()
        {
            return List("Camera Sway", {
                Action("Off", ModelIndex::CameraCrossedOut),
                Action("On", ModelIndex::Camera),
            });
        }

        MenuNode Graphics(const std::vector<std::string>& resolutionLabels)
        {
            // New - not in the original game. "Quality" ties to
            // RenderSettings; "Resolution" ties to ResolutionSettings.
            std::vector<MenuNode> resolutionChildren;
            for (const auto& label : resolutionLabels) { resolutionChildren.push_back(Action(label)); }

            return List("Graphics", {
                List("Quality", {
                    Action("Original"),
                    Action("Smoothed"),
                }),
                List("Resolution", std::move(resolutionChildren)),
            });
        }

        MenuNode LanguageMenu()
        {
            return List("Language", {
                Action("English"),          // English
                Action("Français"),         // French
                Action("Italiano"),         // Italian
                Action("Español"),          // Spanish
                Action("Deutsch"),          // German
                Action("Japanese 日本語"),   // Japanese
            });
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

        MenuNode Options(const std::vector<std::string>& resolutionLabels)
        {
            // modelIndex Computer here is inherited by every child that
            // doesn't set its own - matches the reference images, where
            // the monitor+tower model is the default across almost all of
            // Options and only changes for Controls > Keyboard > Redefine.
            return List("Options", {
                Volume(),
                Controls(),
                Difficulty(),
                CameraSway(),
                Graphics(resolutionLabels),
                LanguageMenu(),
                Credits(),
            }, ModelIndex::Computer);
        }
    }

    MenuNode BuildMainMenuTree(const std::vector<std::string>& resolutionLabels)
    {
        return List("Main Menu", {
            Action("Start Game"),
            Action("Multiplayer"),
            Action("Load Game"),
            Options(resolutionLabels),
        });
    }
}
