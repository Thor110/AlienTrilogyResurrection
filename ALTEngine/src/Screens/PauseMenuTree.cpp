#include "PauseMenuTree.h"
#include "../Formats/ModelIndices.h"
#include "../Menu/MenuTree.h" // for ModelIndex::HarddriveLeft/HarddriveRight

namespace ALTEngine::Screens
{
    using ALTEngine::Menu::MakeAction;
    using ALTEngine::Menu::MakeList;
    using ALTEngine::Menu::MakeSlider;
    using ALTEngine::Menu::MenuNode;
    using ALTEngine::Menu::ModelSource;
    namespace PM = ALTEngine::Formats::ModelIndices::PickMod;

    Menu::MenuNode BuildPauseMenuTree()
    {
        return MakeList("PauseMenu", {
            MakeAction("Auto Mapper", PM::AutoMapper),
            MakeAction("Shoulder Lamp", PM::ShoulderLamp),
            MakeAction("9mm Pistol", PM::Pistol, PM::PistolClip),
            MakeAction("Shotgun", PM::Shotgun, PM::ShotgunShell),
            MakeAction("Flamethrower", PM::Flamethrower, PM::FlamethrowerFuel),
            MakeAction("Pulse Rifle", PM::PulseRifle, PM::PulseRifleClip),
            MakeAction("Smart Gun", PM::SmartGun, PM::SmartGunAmmunition),
            MakeAction("Batteries", PM::Battery),
            MakeAction("Mission"), // no model - shows the mission brief text instead, see PauseMenuScreen
            // HarddriveLeft ("Hard Drive Saving <-") / HarddriveRight
            // ("Hard Drive Loading ->") - same OPTOBJ models SaveSlotScreen
            // shows, matching Save/Load semantics exactly. These are the
            // only two entries in this whole tree that aren't PICKMOD,
            // hence ModelSource::Optobj (Edward, 2026).
            MakeAction("Save Game", ALTEngine::Menu::ModelIndex::HarddriveLeft, -1, ModelSource::Optobj),
            MakeAction("Load Game", ALTEngine::Menu::ModelIndex::HarddriveRight, -1, ModelSource::Optobj),
            MakeList("Options", {
                MakeSlider("SFX Volume"),
                MakeSlider("Music Volume"),
                MakeList("Exit Game", {
                    MakeAction("No"),
                    MakeAction("Yes"),
                }),
            }),
        });
    }
}
