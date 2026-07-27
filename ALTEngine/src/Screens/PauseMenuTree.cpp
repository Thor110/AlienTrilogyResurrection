#include "PauseMenuTree.h"
#include "../Formats/ModelIndices.h"
#include "../Menu/MenuTree.h" // for ModelIndex::HarddriveLeft/HarddriveRight
#include "../Bootstrap/Strings.h"

namespace ALTEngine::Screens
{
    using ALTEngine::Menu::MakeAction;
    using ALTEngine::Menu::MakeList;
    using ALTEngine::Menu::MakeSlider;
    using ALTEngine::Menu::MenuNode;
    using ALTEngine::Menu::ModelSource;
    using ALTEngine::Bootstrap::StringId;
    namespace PM = ALTEngine::Formats::ModelIndices::PickMod;

    namespace
    {
        // Sets stringId after construction, rather than adding new
        // MakeAction/MakeList overloads for a tree this size (16 nodes) -
        // label stays the plain English string, the stable internal
        // identifier PauseMenuScreen's own comparisons rely on
        // (Edward, 2026 - localization foundations).
        MenuNode Tagged(MenuNode node, StringId id)
        {
            node.stringId = static_cast<int>(id);
            return node;
        }
    }

    Menu::MenuNode BuildPauseMenuTree(ALTEngine::Bootstrap::Language)
    {
        return MakeList("PauseMenu", {
            Tagged(MakeAction("Auto Mapper", PM::AutoMapper), StringId::AutoMapper),
            Tagged(MakeAction("Shoulder Lamp", PM::ShoulderLamp), StringId::ShoulderLamp),
            Tagged(MakeAction("9mm Pistol", PM::Pistol, PM::PistolClip), StringId::Pistol9mm),
            Tagged(MakeAction("Shotgun", PM::Shotgun, PM::ShotgunShell), StringId::Shotgun),
            Tagged(MakeAction("Flamethrower", PM::Flamethrower, PM::FlamethrowerFuel), StringId::Flamethrower),
            Tagged(MakeAction("Pulse Rifle", PM::PulseRifle, PM::PulseRifleClip), StringId::PulseRifle),
            Tagged(MakeAction("Smart Gun", PM::SmartGun, PM::SmartGunAmmunition), StringId::SmartGun),
            Tagged(MakeAction("Batteries", PM::Battery), StringId::Batteries),
            Tagged(MakeAction("Mission"), StringId::Mission), // no model - shows the mission brief text instead, see PauseMenuScreen
            // HarddriveLeft ("Hard Drive Saving <-") / HarddriveRight
            // ("Hard Drive Loading ->") - same OPTOBJ models SaveSlotScreen
            // shows, matching Save/Load semantics exactly. These are the
            // only two entries in this whole tree that aren't PICKMOD,
            // hence ModelSource::Optobj (Edward, 2026).
            Tagged(MakeAction("Save Game", ALTEngine::Menu::ModelIndex::HarddriveLeft, -1, ModelSource::Optobj), StringId::SaveGame),
            Tagged(MakeAction("Load Game", ALTEngine::Menu::ModelIndex::HarddriveRight, -1, ModelSource::Optobj), StringId::LoadGame),
            Tagged(MakeList("Options", {
                Tagged(MakeSlider("SFX Volume"), StringId::SfxVolume),
                Tagged(MakeSlider("Music Volume"), StringId::MusicVolume),
                Tagged(MakeList("Exit Game", {
                    Tagged(MakeAction("No"), StringId::No),
                    Tagged(MakeAction("Yes"), StringId::Yes),
                }), StringId::ExitGame),
            }), StringId::Options),
        });
    }
}
