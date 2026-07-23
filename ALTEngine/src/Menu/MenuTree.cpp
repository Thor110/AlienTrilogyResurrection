#include "MenuTree.h"

#include <string>
#include <utility>
#include <vector>

namespace ALTEngine::Menu
{
    // OPTOBJ.BND model indices - CONFIRMED against the real OPTOBJ.BND
    // (Edward, 2026): all 14 sections' identifier bytes match this list
    // exactly, in exact section order (M000-M013 = index 0-13). Also
    // cross-confirmed independently by ModelRenderer.cs's own header-byte
    // comment before that, and originally guessed correctly from that
    // same comment's ordering before either confirmation existed.
    //
    // Real-world device names for indices 2/3/13 use the actual original
    // button text (SpaceOrb 360, Gravis Grip/Gravis Pad, VFX-1) rather
    // than the generic visual-impression names (Gamepad/Multitap/
    // Headphones) Edward had initially used purely from looking at the
    // models - see the per-constant comments below.
    //
    // See Formats/ModelIndices.h for the full three-catalog reference
    // (OBJ3D/PICKMOD/OPTOBJ) salvaged alongside this.
    namespace ModelIndex
    {
        constexpr int Joystick = 0;
        constexpr int Camera = 1;
        constexpr int Gamepad = 2;  // real device: SpaceOrb 360 (confirmed original button text) - "Gamepad" was Edward's visual-impression label from looking at the model, not the actual device name
        constexpr int Multitap = 3; // real devices: Gravis Grip AND Gravis Pad (confirmed original button text) - both use this index, since both look like a multitap; "Multitap" was likewise a visual impression, not necessarily the model's actual real-world identity
        constexpr int HarddriveLeft = 4;  // Hard Drive Saving <-
        constexpr int HarddriveRight = 5; // Hard Drive Loading ->
        constexpr int CameraCrossedOut = 6;
        constexpr int Keyboard = 7;
        constexpr int Mouse = 8;
        constexpr int Computer = 9; // Computer, Monitor and Keyboard
        constexpr int NetworkedComputers = 10; // Two Linked Computers, Monitors and Keyboards (Multiplayer)
        constexpr int SpeakerMusic = 11; // Speaker (Disc Music)
        constexpr int SpeakerSfx = 12;   // Speaker (Sound Effects)
        constexpr int Headphones = 13;   // real device: VFX-1 (confirmed original button text) - "Headphones" was Edward's visual-impression label; VFX-1 was actually a VR headset
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
                // modelIndex on the List node itself (not the Redefine
                // child) - EffectiveModelIndex takes the deepest SET
                // index along the current path, so this shows the
                // correct model the moment Keyboard/Mouse is highlighted,
                // not only after pressing Enter into Redefine. Matches
                // the reference images, where highlighting alone changes
                // the background model (Edward, 2026).
                List("Keyboard", { Action("Redefine") }, ModelIndex::Keyboard),
                List("Mouse", { Action("Redefine") }, ModelIndex::Mouse),
                Action("Joystick", ModelIndex::Joystick),
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
