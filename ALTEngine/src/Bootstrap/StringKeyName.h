#pragma once

#include "StringId.h"

#include <optional>
#include <string>

namespace ALTEngine::Bootstrap
{
    // Stable, human/script-friendly key name for a StringId, used as the
    // key in a language pack's strings.txt (e.g. StringId::StartGame <->
    // "StartGame"). Deliberately NOT positional/ordinal - a hand-edited
    // or script-generated pack file can have keys in any order, add new
    // ones, or be missing others entirely, without anything shifting
    // silently the way a plain array index would (Edward, 2026:
    // language packs need to be robust to editing, not just internally
    // consistent).
    //
    // Regenerated from StringId.h by a script (not hand-maintained) -
    // whenever a StringId is added, this file must be regenerated too,
    // or the new entry silently won't round-trip through any pack file.
    inline const char* StringKeyName(StringId id)
    {
        switch (id)
        {
        case StringId::StartGame: return "StartGame";
        case StringId::Multiplayer: return "Multiplayer";
        case StringId::LoadGame: return "LoadGame";
        case StringId::Options: return "Options";
        case StringId::Volume: return "Volume";
        case StringId::Music: return "Music";
        case StringId::Sfx: return "Sfx";
        case StringId::Controls: return "Controls";
        case StringId::Keyboard: return "Keyboard";
        case StringId::Mouse: return "Mouse";
        case StringId::Joystick: return "Joystick";
        case StringId::GravisGrip: return "GravisGrip";
        case StringId::GravisPad: return "GravisPad";
        case StringId::SpaceOrb360: return "SpaceOrb360";
        case StringId::Vfx1: return "Vfx1";
        case StringId::Difficulty: return "Difficulty";
        case StringId::AcidReign: return "AcidReign";
        case StringId::RagingTerror: return "RagingTerror";
        case StringId::Xenomania: return "Xenomania";
        case StringId::CameraSway: return "CameraSway";
        case StringId::Off: return "Off";
        case StringId::On: return "On";
        case StringId::Graphics: return "Graphics";
        case StringId::Quality: return "Quality";
        case StringId::Original: return "Original";
        case StringId::Smoothed: return "Smoothed";
        case StringId::Resolution: return "Resolution";
        case StringId::LanguageMenuTitle: return "LanguageMenuTitle";
        case StringId::Credits: return "Credits";
        case StringId::LanguageEnglish: return "LanguageEnglish";
        case StringId::LanguageFrench: return "LanguageFrench";
        case StringId::LanguageItalian: return "LanguageItalian";
        case StringId::LanguageSpanish: return "LanguageSpanish";
        case StringId::LanguageGerman: return "LanguageGerman";
        case StringId::LanguageJapanese: return "LanguageJapanese";
        case StringId::Redefine: return "Redefine";
        case StringId::RestoreDefaults: return "RestoreDefaults";
        case StringId::AreYouSure: return "AreYouSure";
        case StringId::Yes: return "Yes";
        case StringId::No: return "No";
        case StringId::ActionMoveForward: return "ActionMoveForward";
        case StringId::ActionMoveBackward: return "ActionMoveBackward";
        case StringId::ActionStrafeLeft: return "ActionStrafeLeft";
        case StringId::ActionStrafeRight: return "ActionStrafeRight";
        case StringId::ActionFire1: return "ActionFire1";
        case StringId::ActionFire2: return "ActionFire2";
        case StringId::ActionUse: return "ActionUse";
        case StringId::ActionStrafeModifier: return "ActionStrafeModifier";
        case StringId::ActionRunMode: return "ActionRunMode";
        case StringId::ActionRunModifier: return "ActionRunModifier";
        case StringId::ActionSelectWeapon1: return "ActionSelectWeapon1";
        case StringId::ActionSelectWeapon2: return "ActionSelectWeapon2";
        case StringId::ActionSelectWeapon3: return "ActionSelectWeapon3";
        case StringId::ActionSelectWeapon4: return "ActionSelectWeapon4";
        case StringId::ActionSelectWeapon5: return "ActionSelectWeapon5";
        case StringId::ActionNextWeapon: return "ActionNextWeapon";
        case StringId::ActionTurnaround: return "ActionTurnaround";
        case StringId::ActionWeaponSelectMenu: return "ActionWeaponSelectMenu";
        case StringId::ActionPause: return "ActionPause";
        case StringId::AutoMapper: return "AutoMapper";
        case StringId::ShoulderLamp: return "ShoulderLamp";
        case StringId::Pistol9mm: return "Pistol9mm";
        case StringId::Shotgun: return "Shotgun";
        case StringId::Flamethrower: return "Flamethrower";
        case StringId::PulseRifle: return "PulseRifle";
        case StringId::SmartGun: return "SmartGun";
        case StringId::Batteries: return "Batteries";
        case StringId::Mission: return "Mission";
        case StringId::SaveGame: return "SaveGame";
        case StringId::ExitGame: return "ExitGame";
        case StringId::SfxVolume: return "SfxVolume";
        case StringId::MusicVolume: return "MusicVolume";
        case StringId::PressEscToGoBack: return "PressEscToGoBack";
        case StringId::PressEnterToSelect: return "PressEnterToSelect";
        case StringId::CreditsTitle: return "CreditsTitle";
        case StringId::CreditsPlaceholder: return "CreditsPlaceholder";
        case StringId::PressAKeyToBind: return "PressAKeyToBind";
        case StringId::PressAMouseButtonOrWheelToBind: return "PressAMouseButtonOrWheelToBind";
        case StringId::OrEscToCancel: return "OrEscToCancel";
        case StringId::OptionsTitle: return "OptionsTitle";
        case StringId::NotAvailable: return "NotAvailable";
        case StringId::ExitGameTitle: return "ExitGameTitle";
        case StringId::VSync: return "VSync";
        case StringId::DisplayModeTitle: return "DisplayModeTitle";
        case StringId::Windowed: return "Windowed";
        case StringId::Fullscreen: return "Fullscreen";
        case StringId::Borderless: return "Borderless";
        case StringId::MouseSensitivity: return "MouseSensitivity";
        case StringId::Modern: return "Modern";
        case StringId::EnableAll: return "EnableAll";
        case StringId::AutomaticDoors: return "AutomaticDoors";
        case StringId::Custom: return "Custom";
        case StringId::KeepItems: return "KeepItems";
        default: return "";
        }
    }

    // Inverse lookup - std::nullopt if `name` isn't a recognised key
    // (e.g. a stale key from an older version of this engine, or a typo
    // in a hand-edited pack file). Callers should skip unrecognised keys
    // silently rather than fail the whole pack load over one bad line.
    inline std::optional<StringId> StringIdFromKeyName(const std::string& name)
    {
        if (name == "StartGame") { return StringId::StartGame; }
        if (name == "Multiplayer") { return StringId::Multiplayer; }
        if (name == "LoadGame") { return StringId::LoadGame; }
        if (name == "Options") { return StringId::Options; }
        if (name == "Volume") { return StringId::Volume; }
        if (name == "Music") { return StringId::Music; }
        if (name == "Sfx") { return StringId::Sfx; }
        if (name == "Controls") { return StringId::Controls; }
        if (name == "Keyboard") { return StringId::Keyboard; }
        if (name == "Mouse") { return StringId::Mouse; }
        if (name == "Joystick") { return StringId::Joystick; }
        if (name == "GravisGrip") { return StringId::GravisGrip; }
        if (name == "GravisPad") { return StringId::GravisPad; }
        if (name == "SpaceOrb360") { return StringId::SpaceOrb360; }
        if (name == "Vfx1") { return StringId::Vfx1; }
        if (name == "Difficulty") { return StringId::Difficulty; }
        if (name == "AcidReign") { return StringId::AcidReign; }
        if (name == "RagingTerror") { return StringId::RagingTerror; }
        if (name == "Xenomania") { return StringId::Xenomania; }
        if (name == "CameraSway") { return StringId::CameraSway; }
        if (name == "Off") { return StringId::Off; }
        if (name == "On") { return StringId::On; }
        if (name == "Graphics") { return StringId::Graphics; }
        if (name == "Quality") { return StringId::Quality; }
        if (name == "Original") { return StringId::Original; }
        if (name == "Smoothed") { return StringId::Smoothed; }
        if (name == "Resolution") { return StringId::Resolution; }
        if (name == "LanguageMenuTitle") { return StringId::LanguageMenuTitle; }
        if (name == "Credits") { return StringId::Credits; }
        if (name == "LanguageEnglish") { return StringId::LanguageEnglish; }
        if (name == "LanguageFrench") { return StringId::LanguageFrench; }
        if (name == "LanguageItalian") { return StringId::LanguageItalian; }
        if (name == "LanguageSpanish") { return StringId::LanguageSpanish; }
        if (name == "LanguageGerman") { return StringId::LanguageGerman; }
        if (name == "LanguageJapanese") { return StringId::LanguageJapanese; }
        if (name == "Redefine") { return StringId::Redefine; }
        if (name == "RestoreDefaults") { return StringId::RestoreDefaults; }
        if (name == "AreYouSure") { return StringId::AreYouSure; }
        if (name == "Yes") { return StringId::Yes; }
        if (name == "No") { return StringId::No; }
        if (name == "ActionMoveForward") { return StringId::ActionMoveForward; }
        if (name == "ActionMoveBackward") { return StringId::ActionMoveBackward; }
        if (name == "ActionStrafeLeft") { return StringId::ActionStrafeLeft; }
        if (name == "ActionStrafeRight") { return StringId::ActionStrafeRight; }
        if (name == "ActionFire1") { return StringId::ActionFire1; }
        if (name == "ActionFire2") { return StringId::ActionFire2; }
        if (name == "ActionUse") { return StringId::ActionUse; }
        if (name == "ActionStrafeModifier") { return StringId::ActionStrafeModifier; }
        if (name == "ActionRunMode") { return StringId::ActionRunMode; }
        if (name == "ActionRunModifier") { return StringId::ActionRunModifier; }
        if (name == "ActionSelectWeapon1") { return StringId::ActionSelectWeapon1; }
        if (name == "ActionSelectWeapon2") { return StringId::ActionSelectWeapon2; }
        if (name == "ActionSelectWeapon3") { return StringId::ActionSelectWeapon3; }
        if (name == "ActionSelectWeapon4") { return StringId::ActionSelectWeapon4; }
        if (name == "ActionSelectWeapon5") { return StringId::ActionSelectWeapon5; }
        if (name == "ActionNextWeapon") { return StringId::ActionNextWeapon; }
        if (name == "ActionTurnaround") { return StringId::ActionTurnaround; }
        if (name == "ActionWeaponSelectMenu") { return StringId::ActionWeaponSelectMenu; }
        if (name == "ActionPause") { return StringId::ActionPause; }
        if (name == "AutoMapper") { return StringId::AutoMapper; }
        if (name == "ShoulderLamp") { return StringId::ShoulderLamp; }
        if (name == "Pistol9mm") { return StringId::Pistol9mm; }
        if (name == "Shotgun") { return StringId::Shotgun; }
        if (name == "Flamethrower") { return StringId::Flamethrower; }
        if (name == "PulseRifle") { return StringId::PulseRifle; }
        if (name == "SmartGun") { return StringId::SmartGun; }
        if (name == "Batteries") { return StringId::Batteries; }
        if (name == "Mission") { return StringId::Mission; }
        if (name == "SaveGame") { return StringId::SaveGame; }
        if (name == "ExitGame") { return StringId::ExitGame; }
        if (name == "SfxVolume") { return StringId::SfxVolume; }
        if (name == "MusicVolume") { return StringId::MusicVolume; }
        if (name == "PressEscToGoBack") { return StringId::PressEscToGoBack; }
        if (name == "PressEnterToSelect") { return StringId::PressEnterToSelect; }
        if (name == "CreditsTitle") { return StringId::CreditsTitle; }
        if (name == "CreditsPlaceholder") { return StringId::CreditsPlaceholder; }
        if (name == "PressAKeyToBind") { return StringId::PressAKeyToBind; }
        if (name == "PressAMouseButtonOrWheelToBind") { return StringId::PressAMouseButtonOrWheelToBind; }
        if (name == "OrEscToCancel") { return StringId::OrEscToCancel; }
        if (name == "OptionsTitle") { return StringId::OptionsTitle; }
        if (name == "NotAvailable") { return StringId::NotAvailable; }
        if (name == "ExitGameTitle") { return StringId::ExitGameTitle; }
        if (name == "VSync") { return StringId::VSync; }
        if (name == "DisplayModeTitle") { return StringId::DisplayModeTitle; }
        if (name == "Windowed") { return StringId::Windowed; }
        if (name == "Fullscreen") { return StringId::Fullscreen; }
        if (name == "Borderless") { return StringId::Borderless; }
        if (name == "MouseSensitivity") { return StringId::MouseSensitivity; }
        if (name == "Modern") { return StringId::Modern; }
        if (name == "EnableAll") { return StringId::EnableAll; }
        if (name == "AutomaticDoors") { return StringId::AutomaticDoors; }
        if (name == "Custom") { return StringId::Custom; }
        if (name == "KeepItems") { return StringId::KeepItems; }
        return std::nullopt;
    }
}
