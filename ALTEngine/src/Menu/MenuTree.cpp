#include "MenuTree.h"

#include <string>
#include <utility>
#include <vector>

namespace ALTEngine::Menu
{
    // Provisional OPTOBJ model index assignment. NOT extracted from real
    // data - we don't have OPTOBJ.BND to confirm actual on-disk section
    // order against. Numbered in the order ModelRenderer.cs's comment
    // block happens to list the known identifiers (Joystick, Camera,
    // Controller, Gravis Grip Controller, Harddrive<-, Harddrive->,
    // Camera X, Keyboard, Mouse, Computer, Networked Computers, Speaker
    // Music, Speaker SFX, Headphones) - that ordering is an artifact of
    // how the comment happened to be written, not a confirmed index.
    //
    // This IS the documentation of what each index means for override
    // authors (Edward's plan: a text file in the Override folder at
    // release, listing e.g. "OPTOBJ_09 = Computer"). Keep this list and
    // that document in sync if either changes.
    namespace ModelIndex
    {
        constexpr int Joystick = 0;
        constexpr int Camera = 1;
        constexpr int Controller = 2;
        constexpr int GravisGrip = 3;
        constexpr int HarddriveLeft = 4;
        constexpr int HarddriveRight = 5;
        constexpr int CameraX = 6;
        constexpr int Keyboard = 7;
        constexpr int Mouse = 8;
        constexpr int Computer = 9;
        constexpr int NetworkedComputers = 10;
        constexpr int SpeakerMusic = 11;
        constexpr int SpeakerSfx = 12;
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
                Action("Gravis Grip", ModelIndex::GravisGrip),
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

        MenuNode RenderQuality()
        {
            // New - not in the original game. Ties to RenderSettings.
            return List("Render Quality", {
                Action("Original"),
                Action("Smoothed"),
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

        MenuNode Options()
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
                RenderQuality(),
                LanguageMenu(),
                Credits(),
            }, ModelIndex::Computer);
        }
    }

    MenuNode BuildMainMenuTree()
    {
        return List("Main Menu", {
            Action("Start Game"),
            Action("Multiplayer"),
            Action("Load Game"),
            Options(),
        });
    }
}
