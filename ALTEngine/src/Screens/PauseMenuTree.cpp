#include "PauseMenuTree.h"
#include "../Formats/ModelIndices.h"
#include "../Menu/MenuTree.h" // for ModelIndex::HarddriveLeft/HarddriveRight
#include "../Bootstrap/Strings.h"

namespace ALTEngine::Screens
{
    using ALTEngine::Menu::MakeAction;
    using ALTEngine::Menu::MakeList;
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
            // Options is now a plain trigger (Edward, 2026: "update the
            // options button in the pause menu so that it opens the
            // [options] menu") - PauseMenuScreen intercepts it specially
            // and launches MenuController::Run directly into the same
            // Options tree the boot menu uses, same pattern Save Game/
            // Load Game already use for SaveSlotScreen. No children of
            // its own here anymore - SFX/Music Volume moved into that
            // real Options menu (removed from here entirely, not
            // duplicated), and Exit Game moved out to the top level
            // below.
            Tagged(MakeAction("Options"), StringId::Options),
            // Cheats, between Options and Exit Game. Present only when the
            // Modern "Enable Cheats" feature is on, so a normal game never sees
            // it (Edward, 2026 - useful for testing as much as for players).
            //
            // Each cheat is a plain action; PauseMenuScreen applies it and the
            // list is built here so adding one is a single entry plus its
            // StringIds.
            // Cheats, between Options and Exit Game. Present only when the
            // Modern "Enable Cheats" feature is on - PauseMenuScreen removes it
            // otherwise, and re-checks every time the menu opens so it can be
            // turned on or off mid-game.
            //
            // Each cheat is its OWN list with Off/On beneath it, the same shape
            // as a Modern feature, so it toggles rather than firing once and its
            // description lands in the standard place (Edward, 2026).
            // Cheats -> Fully Loaded -> No / Yes, all buttons, the same shape as
            // Exit Game's confirm rather than a settings toggle (Edward, 2026).
            // Present only when the Modern "Enable Cheats" feature is on;
            // PauseMenuScreen adds and removes it, re-checking every time the
            // menu opens and again on return from Options.
            Tagged(MakeList("Cheats", {
                // Enable All first, working the same way Modern's does: it turns
                // every cheat below it on or off in one go (Edward, 2026).
                Tagged(MakeList("Enable All", {
                    Tagged(MakeAction("Off"), StringId::Off),
                    Tagged(MakeAction("On"), StringId::On),
                }), StringId::EnableAll),
                Tagged(MakeList("Fully Loaded", {
                    Tagged(MakeAction("Off"), StringId::Off),
                    Tagged(MakeAction("On"), StringId::On),
                }), StringId::FullyLoaded),
                Tagged(MakeList("Maximum Health", {
                    Tagged(MakeAction("Off"), StringId::Off),
                    Tagged(MakeAction("On"), StringId::On),
                }), StringId::MaximumHealth),
            }), StringId::Cheats),
            Tagged(MakeList("Exit Game", {
                Tagged(MakeAction("No"), StringId::No),
                Tagged(MakeAction("Yes"), StringId::Yes),
            }), StringId::ExitGame),
        });
    }
}
