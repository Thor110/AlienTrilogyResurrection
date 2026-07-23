#include "MenuTree.h"

#include <string>
#include <utility>
#include <vector>

namespace ALTEngine::Menu
{
    // OPTOBJ.BND model indices - CONFIRMED (Edward's salvaged comments
    // from prior work, 2026), not the guessed ordering this used to be.
    // Interestingly, the guess (derived from ModelRenderer.cs's comment
    // ordering) matched this exactly, index for index - kept as-is, just
    // a couple of names corrected to match precisely (Controller ->
    // Gamepad, GravisGrip -> Multitap, CameraX -> CameraCrossedOut).
    // See Formats/ModelIndices.h for the full three-catalog reference
    // (OBJ3D/PICKMOD/OPTOBJ) salvaged alongside this.
    namespace ModelIndex
    {
        constexpr int Joystick = 0;
        constexpr int Camera = 1;
        constexpr int Gamepad = 2;
        constexpr int Multitap = 3; // "Multitap?" in the source comment - noted as uncertain there
        constexpr int HarddriveLeft = 4;  // Hard Drive Saving <-
        constexpr int HarddriveRight = 5; // Hard Drive Loading ->
        constexpr int CameraCrossedOut = 6;
        constexpr int Keyboard = 7;
        constexpr int Mouse = 8;
        constexpr int Computer = 9; // Computer, Monitor and Keyboard
        constexpr int NetworkedComputers = 10; // Two Linked Computers, Monitors and Keyboards (Multiplayer)
        constexpr int SpeakerMusic = 11; // Speaker (Disc Music)
        constexpr int SpeakerSfx = 12;   // Speaker (Sound Effects)
        constexpr int Headphones = 13;
    }

    namespace
    {
        MenuNode Action(std::string label, int modelIndex = -1)
        {
            MenuNode n;
            n.label = std::move(label);
            n.kind = MenuNodeKind::Action;
            n.modelIndex = modelIndex;
            return n;
        }

        MenuNode List(std::string label, std::vector<MenuNode> children, int modelIndex = -1)
        {
            MenuNode n;
            n.label = std::move(label);
            n.kind = MenuNodeKind::List;
            n.modelIndex = modelIndex;
            n.children = std::move(children);
            return n;
        }

        MenuNode Controls()
        {
            return List("Controls", {
                List("Keyboard", { Action("Redefine", ModelIndex::Keyboard) }),
                List("Mouse", { Action("Redefine", ModelIndex::Mouse) }),
                Action("Joystick", ModelIndex::Joystick),
                // Model index deliberately left unset here - index 3 in
                // the confirmed OPTOBJ list is "Multitap?" (uncertain
                // even in the source comment), and a Multitap (a
                // controller-port expander) isn't the same device as a
                // Gravis Grip (a joystick) - no reason to assume they
                // share a model just because both happened to be
                // uncertain. Falls back to the inherited "Computer".
                Action("Gravis Grip"),
                Action("Gravis Pad"),   // no reference image confirming a distinct model
                Action("SpaceOrb 360"), // no reference image confirming a distinct model
                Action("VFX-1"),        // no reference image confirming a distinct model
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
                Action("Off"),
                Action("On"),
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
                Action("English"),
                Action("Français"),
                Action("Italiano"),
                Action("Español"),
            });
        }

        MenuNode Volume()
        {
            MenuNode n;
            n.label = "Volume";
            n.kind = MenuNodeKind::Slider;
            return n;
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
