#include "PauseMenuTree.h"
#include "../Formats/ModelIndices.h"

namespace ALTEngine::Screens
{
    using ALTEngine::Menu::MakeAction;
    using ALTEngine::Menu::MakeList;
    using ALTEngine::Menu::MakeSlider;
    using ALTEngine::Menu::MenuNode;
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
