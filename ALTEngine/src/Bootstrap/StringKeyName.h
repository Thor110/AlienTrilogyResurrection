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
        case StringId::ActionTurnLeft: return "ActionTurnLeft";
        case StringId::ActionTurnRight: return "ActionTurnRight";
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
        case StringId::Available: return "Available";
        case StringId::Selected: return "Selected";
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
        case StringId::SkipEndLevelScreen: return "SkipEndLevelScreen";
        case StringId::PlayerJumping: return "PlayerJumping";
        case StringId::StunnedEnemies: return "StunnedEnemies";
        case StringId::DescEnableAll: return "DescEnableAll";
        case StringId::DescAutomaticDoors: return "DescAutomaticDoors";
        case StringId::DescKeepItems: return "DescKeepItems";
        case StringId::DescSkipEndLevel: return "DescSkipEndLevel";
        case StringId::DescPlayerJumping: return "DescPlayerJumping";
        case StringId::DescStunnedEnemies: return "DescStunnedEnemies";
        case StringId::FreeLook: return "FreeLook";
        case StringId::DescFreeLook: return "DescFreeLook";
        case StringId::RenderDistance: return "RenderDistance";
        case StringId::DescRenderDistance: return "DescRenderDistance";
        case StringId::LevelSelect: return "LevelSelect";
        case StringId::DescLevelSelect: return "DescLevelSelect";
        case StringId::LiveMinimap: return "LiveMinimap";
        case StringId::DescLiveMinimap: return "DescLiveMinimap";
        case StringId::PresetConvenience: return "PresetConvenience";
        case StringId::DescPresetConvenience: return "DescPresetConvenience";
        case StringId::PresetModernised: return "PresetModernised";
        case StringId::DescPresetModernised: return "DescPresetModernised";
        case StringId::PresetTesting: return "PresetTesting";
        case StringId::DescPresetTesting: return "DescPresetTesting";
        case StringId::EnableCheats: return "EnableCheats";
        case StringId::DescEnableCheats: return "DescEnableCheats";
        case StringId::Cheats: return "Cheats";
        case StringId::FullyLoaded: return "FullyLoaded";
        case StringId::DescFullyLoaded: return "DescFullyLoaded";
        case StringId::MaximumHealth: return "MaximumHealth";
        case StringId::DescMaximumHealth: return "DescMaximumHealth";
        case StringId::GsOptions: return "GsOptions";
        case StringId::GsRedefineKeyboard: return "GsRedefineKeyboard";
        case StringId::GsRedefineMouse: return "GsRedefineMouse";
        case StringId::GsPressEscToGoBack: return "GsPressEscToGoBack";
        case StringId::GsPressEnterToAdjust: return "GsPressEnterToAdjust";
        case StringId::GsPressEnterToSelect: return "GsPressEnterToSelect";
        case StringId::GsDefaultPlayerName: return "GsDefaultPlayerName";
        case StringId::GsCheatInvincible: return "GsCheatInvincible";
        case StringId::GsCheatAllWeapons: return "GsCheatAllWeapons";
        case StringId::GsCheatAllAmmo: return "GsCheatAllAmmo";
        case StringId::GsCheatGotoLevel: return "GsCheatGotoLevel";
        case StringId::GsCheatMaxLevel: return "GsCheatMaxLevel";
        case StringId::GsCheatSetEverything: return "GsCheatSetEverything";
        case StringId::GsCheatWrongGame: return "GsCheatWrongGame";
        case StringId::GsCheatDoomed: return "GsCheatDoomed";
        case StringId::GsCheatYouWish: return "GsCheatYouWish";
        case StringId::GsCheatLookingCool: return "GsCheatLookingCool";
        case StringId::GsCheatDuking: return "GsCheatDuking";
        case StringId::GsCheatThatsYou: return "GsCheatThatsYou";
        case StringId::GsCheatNiceTry: return "GsCheatNiceTry";
        case StringId::GsCheatOi: return "GsCheatOi";
        case StringId::GsVolume: return "GsVolume";
        case StringId::GsControls: return "GsControls";
        case StringId::GsDifficulty: return "GsDifficulty";
        case StringId::GsCameraSway: return "GsCameraSway";
        case StringId::GsCredits: return "GsCredits";
        case StringId::GsMusic: return "GsMusic";
        case StringId::GsSfx: return "GsSfx";
        case StringId::GsKeyboard: return "GsKeyboard";
        case StringId::GsMouse: return "GsMouse";
        case StringId::GsJoystick: return "GsJoystick";
        case StringId::GsGravisGrip: return "GsGravisGrip";
        case StringId::GsGravisPad: return "GsGravisPad";
        case StringId::GsSpaceOrb: return "GsSpaceOrb";
        case StringId::GsVfx1: return "GsVfx1";
        case StringId::GsDifficultyEasy: return "GsDifficultyEasy";
        case StringId::GsDifficultyMedium: return "GsDifficultyMedium";
        case StringId::GsDifficultyHard: return "GsDifficultyHard";
        case StringId::GsOff: return "GsOff";
        case StringId::GsOn: return "GsOn";
        case StringId::GsRedefine: return "GsRedefine";
        case StringId::GsCalibrate: return "GsCalibrate";
        case StringId::GsMoveForwards: return "GsMoveForwards";
        case StringId::GsMoveBackwards: return "GsMoveBackwards";
        case StringId::GsTurnLeft: return "GsTurnLeft";
        case StringId::GsTurnRight: return "GsTurnRight";
        case StringId::GsStrafeLeft: return "GsStrafeLeft";
        case StringId::GsStrafeRight: return "GsStrafeRight";
        case StringId::GsStrafeModifier: return "GsStrafeModifier";
        case StringId::GsFire1: return "GsFire1";
        case StringId::GsFire2: return "GsFire2";
        case StringId::GsDoUse: return "GsDoUse";
        case StringId::GsNextWeapon: return "GsNextWeapon";
        case StringId::GsLookUp: return "GsLookUp";
        case StringId::GsLookDown: return "GsLookDown";
        case StringId::GsTurnAround: return "GsTurnAround";
        case StringId::GsLeftButton: return "GsLeftButton";
        case StringId::GsRightButton: return "GsRightButton";
        case StringId::GsMiddleButton: return "GsMiddleButton";
        case StringId::GsBlank: return "GsBlank";
        case StringId::GsAutoMapper: return "GsAutoMapper";
        case StringId::GsShoulderLamp: return "GsShoulderLamp";
        case StringId::GsWeapon9mmPistol: return "GsWeapon9mmPistol";
        case StringId::GsWeaponShotgun: return "GsWeaponShotgun";
        case StringId::GsWeaponFlamethrower: return "GsWeaponFlamethrower";
        case StringId::GsWeaponPulseRifle: return "GsWeaponPulseRifle";
        case StringId::GsWeaponSmartGun: return "GsWeaponSmartGun";
        case StringId::GsBatteries: return "GsBatteries";
        case StringId::GsMission: return "GsMission";
        case StringId::GsOptionsMenu: return "GsOptionsMenu";
        case StringId::GsSfxVolume: return "GsSfxVolume";
        case StringId::GsCddaVolume: return "GsCddaVolume";
        case StringId::GsExitGame: return "GsExitGame";
        case StringId::GsNightVision: return "GsNightVision";
        case StringId::GsNoAmmoAvailable: return "GsNoAmmoAvailable";
        case StringId::GsRoundsAvailable: return "GsRoundsAvailable";
        case StringId::GsAreYouSure: return "GsAreYouSure";
        case StringId::GsYes: return "GsYes";
        case StringId::GsNo: return "GsNo";
        case StringId::GsSelected: return "GsSelected";
        case StringId::GsAvailable: return "GsAvailable";
        case StringId::GsNotAvailable: return "GsNotAvailable";
        case StringId::GsDoorActivated: return "GsDoorActivated";
        case StringId::GsDoorPoweredUp: return "GsDoorPoweredUp";
        case StringId::GsSteamValveClosed: return "GsSteamValveClosed";
        case StringId::GsFlameJetShutDown: return "GsFlameJetShutDown";
        case StringId::GsLiftActivated: return "GsLiftActivated";
        case StringId::GsBatteryRequired: return "GsBatteryRequired";
        case StringId::GsJoyCentre: return "GsJoyCentre";
        case StringId::GsJoyLowerRight: return "GsJoyLowerRight";
        case StringId::GsFaceForward: return "GsFaceForward";
        case StringId::GsLoadingDataStars: return "GsLoadingDataStars";
        case StringId::GsLoadingData: return "GsLoadingData";
        case StringId::GsHitAnyKey: return "GsHitAnyKey";
        case StringId::GsIncomingTransfer: return "GsIncomingTransfer";
        case StringId::GsMissionBrief: return "GsMissionBrief";
        case StringId::GsPleaseWaitStarting: return "GsPleaseWaitStarting";
        case StringId::GsStartMultiplayer: return "GsStartMultiplayer";
        case StringId::GsJoinMultiplayer: return "GsJoinMultiplayer";
        case StringId::GsMultiplayerOptions: return "GsMultiplayerOptions";
        case StringId::GsStartGame: return "GsStartGame";
        case StringId::GsMultiplayer: return "GsMultiplayer";
        case StringId::GsLoadGame: return "GsLoadGame";
        case StringId::GsContinueGame: return "GsContinueGame";
        case StringId::GsSaveGame: return "GsSaveGame";
        case StringId::GsQuitGame: return "GsQuitGame";
        case StringId::GsMissionAssessment: return "GsMissionAssessment";
        case StringId::GsAliens: return "GsAliens";
        case StringId::GsSecrets: return "GsSecrets";
        case StringId::GsMissionLabel: return "GsMissionLabel";
        case StringId::GsPressAnyKey: return "GsPressAnyKey";
        case StringId::GsWaitingForPlayers: return "GsWaitingForPlayers";
        case StringId::GsScores: return "GsScores";
        case StringId::GsGameSetup: return "GsGameSetup";
        case StringId::GsNameOfGame: return "GsNameOfGame";
        case StringId::GsStartAtLevel: return "GsStartAtLevel";
        case StringId::GsMinimumGameLength: return "GsMinimumGameLength";
        case StringId::GsAcidHell: return "GsAcidHell";
        case StringId::GsSearchingNetGames: return "GsSearchingNetGames";
        case StringId::GsOpen: return "GsOpen";
        case StringId::GsClosed: return "GsClosed";
        case StringId::GsBetweenLevels: return "GsBetweenLevels";
        case StringId::GsLevel: return "GsLevel";
        case StringId::GsPlayers: return "GsPlayers";
        case StringId::GsStatus: return "GsStatus";
        case StringId::GsEditYourData: return "GsEditYourData";
        case StringId::GsYourName: return "GsYourName";
        case StringId::GsFxMessage: return "GsFxMessage";
        case StringId::GsInvalidSaveFile: return "GsInvalidSaveFile";
        case StringId::GsErrorWritingSave: return "GsErrorWritingSave";
        case StringId::GsUnused130: return "GsUnused130";
        case StringId::GsInsertCd: return "GsInsertCd";
        case StringId::GsNoPathTxt: return "GsNoPathTxt";
        case StringId::GsDemo: return "GsDemo";
        case StringId::GsRunMode: return "GsRunMode";
        case StringId::GsBattery: return "GsBattery";
        case StringId::GsReallyQuit: return "GsReallyQuit";
        case StringId::GsPickup9mmAutomatic: return "GsPickup9mmAutomatic";
        case StringId::GsPickupShotgun: return "GsPickupShotgun";
        case StringId::GsPickupPulseRifle: return "GsPickupPulseRifle";
        case StringId::GsPickupFlamethrower: return "GsPickupFlamethrower";
        case StringId::GsPickupSmartGun: return "GsPickupSmartGun";
        case StringId::GsPickupNotUsed: return "GsPickupNotUsed";
        case StringId::GsPickupSeismicCharges: return "GsPickupSeismicCharges";
        case StringId::GsPickupBattery: return "GsPickupBattery";
        case StringId::GsPickupNightVision: return "GsPickupNightVision";
        case StringId::GsPickup9mmClip: return "GsPickup9mmClip";
        case StringId::GsPickupShotgunCartridges: return "GsPickupShotgunCartridges";
        case StringId::GsPickupPulseClip: return "GsPickupPulseClip";
        case StringId::GsPickupGrenades: return "GsPickupGrenades";
        case StringId::GsPickupFlamethrowerFuel: return "GsPickupFlamethrowerFuel";
        case StringId::GsPickupSmartgunAmmo: return "GsPickupSmartgunAmmo";
        case StringId::GsPickupIdentityTag: return "GsPickupIdentityTag";
        case StringId::GsPickupAutoMapper: return "GsPickupAutoMapper";
        case StringId::GsPickupHypoPack: return "GsPickupHypoPack";
        case StringId::GsPickupAcidVest: return "GsPickupAcidVest";
        case StringId::GsPickupBodySuit: return "GsPickupBodySuit";
        case StringId::GsPickupMediKit: return "GsPickupMediKit";
        case StringId::GsPickupDermPatch: return "GsPickupDermPatch";
        case StringId::GsPickupProtectiveBoots: return "GsPickupProtectiveBoots";
        case StringId::GsPickupAdrenalineBurst: return "GsPickupAdrenalineBurst";
        case StringId::GsPickupShoulderLamp: return "GsPickupShoulderLamp";
        case StringId::GsNeed8Mb: return "GsNeed8Mb";
        case StringId::GsFatalServerCrash: return "GsFatalServerCrash";
        case StringId::GsPressReturnToContinue: return "GsPressReturnToContinue";
        case StringId::GsPlayerQuit: return "GsPlayerQuit";
        case StringId::GsPlayerJoined: return "GsPlayerJoined";
        case StringId::GsPlayerSays: return "GsPlayerSays";
        case StringId::GsNotInGame: return "GsNotInGame";
        case StringId::GsKilledHimself: return "GsKilledHimself";
        case StringId::GsYouKilled: return "GsYouKilled";
        case StringId::GsKilledOther: return "GsKilledOther";
        case StringId::GsPlayerCrashed: return "GsPlayerCrashed";
        case StringId::GsExitSwitchActive: return "GsExitSwitchActive";
        case StringId::GsThirtySeconds: return "GsThirtySeconds";
        case StringId::GsSomeoneBeatYou: return "GsSomeoneBeatYou";
        case StringId::GsSending: return "GsSending";
        case StringId::GsSecondsToEvacuate: return "GsSecondsToEvacuate";
        case StringId::GsKilledYou: return "GsKilledYou";
        case StringId::GsYouWereKilled: return "GsYouWereKilled";
        case StringId::GsYouKilledYourself: return "GsYouKilledYourself";
        case StringId::GsLanguage: return "GsLanguage";
        case StringId::GsLangEnglish: return "GsLangEnglish";
        case StringId::GsLangFrench: return "GsLangFrench";
        case StringId::GsLangItalian: return "GsLangItalian";
        case StringId::GsLangSpanish: return "GsLangSpanish";

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
        if (name == "ActionTurnLeft") { return StringId::ActionTurnLeft; }
        if (name == "ActionTurnRight") { return StringId::ActionTurnRight; }
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
        if (name == "Available") { return StringId::Available; }
        if (name == "Selected") { return StringId::Selected; }
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
        if (name == "SkipEndLevelScreen") { return StringId::SkipEndLevelScreen; }
        if (name == "PlayerJumping") { return StringId::PlayerJumping; }
        if (name == "StunnedEnemies") { return StringId::StunnedEnemies; }
        if (name == "DescEnableAll") { return StringId::DescEnableAll; }
        if (name == "DescAutomaticDoors") { return StringId::DescAutomaticDoors; }
        if (name == "DescKeepItems") { return StringId::DescKeepItems; }
        if (name == "DescSkipEndLevel") { return StringId::DescSkipEndLevel; }
        if (name == "DescPlayerJumping") { return StringId::DescPlayerJumping; }
        if (name == "DescStunnedEnemies") { return StringId::DescStunnedEnemies; }
        if (name == "FreeLook") { return StringId::FreeLook; }
        if (name == "DescFreeLook") { return StringId::DescFreeLook; }
        if (name == "RenderDistance") { return StringId::RenderDistance; }
        if (name == "DescRenderDistance") { return StringId::DescRenderDistance; }
        if (name == "LevelSelect") { return StringId::LevelSelect; }
        if (name == "DescLevelSelect") { return StringId::DescLevelSelect; }
        if (name == "LiveMinimap") { return StringId::LiveMinimap; }
        if (name == "DescLiveMinimap") { return StringId::DescLiveMinimap; }
        if (name == "PresetConvenience") { return StringId::PresetConvenience; }
        if (name == "DescPresetConvenience") { return StringId::DescPresetConvenience; }
        if (name == "PresetModernised") { return StringId::PresetModernised; }
        if (name == "DescPresetModernised") { return StringId::DescPresetModernised; }
        if (name == "PresetTesting") { return StringId::PresetTesting; }
        if (name == "DescPresetTesting") { return StringId::DescPresetTesting; }
        if (name == "EnableCheats") { return StringId::EnableCheats; }
        if (name == "DescEnableCheats") { return StringId::DescEnableCheats; }
        if (name == "Cheats") { return StringId::Cheats; }
        if (name == "FullyLoaded") { return StringId::FullyLoaded; }
        if (name == "DescFullyLoaded") { return StringId::DescFullyLoaded; }
        if (name == "MaximumHealth") { return StringId::MaximumHealth; }
        if (name == "DescMaximumHealth") { return StringId::DescMaximumHealth; }
        if (name == "GsOptions") { return StringId::GsOptions; }
        if (name == "GsRedefineKeyboard") { return StringId::GsRedefineKeyboard; }
        if (name == "GsRedefineMouse") { return StringId::GsRedefineMouse; }
        if (name == "GsPressEscToGoBack") { return StringId::GsPressEscToGoBack; }
        if (name == "GsPressEnterToAdjust") { return StringId::GsPressEnterToAdjust; }
        if (name == "GsPressEnterToSelect") { return StringId::GsPressEnterToSelect; }
        if (name == "GsDefaultPlayerName") { return StringId::GsDefaultPlayerName; }
        if (name == "GsCheatInvincible") { return StringId::GsCheatInvincible; }
        if (name == "GsCheatAllWeapons") { return StringId::GsCheatAllWeapons; }
        if (name == "GsCheatAllAmmo") { return StringId::GsCheatAllAmmo; }
        if (name == "GsCheatGotoLevel") { return StringId::GsCheatGotoLevel; }
        if (name == "GsCheatMaxLevel") { return StringId::GsCheatMaxLevel; }
        if (name == "GsCheatSetEverything") { return StringId::GsCheatSetEverything; }
        if (name == "GsCheatWrongGame") { return StringId::GsCheatWrongGame; }
        if (name == "GsCheatDoomed") { return StringId::GsCheatDoomed; }
        if (name == "GsCheatYouWish") { return StringId::GsCheatYouWish; }
        if (name == "GsCheatLookingCool") { return StringId::GsCheatLookingCool; }
        if (name == "GsCheatDuking") { return StringId::GsCheatDuking; }
        if (name == "GsCheatThatsYou") { return StringId::GsCheatThatsYou; }
        if (name == "GsCheatNiceTry") { return StringId::GsCheatNiceTry; }
        if (name == "GsCheatOi") { return StringId::GsCheatOi; }
        if (name == "GsVolume") { return StringId::GsVolume; }
        if (name == "GsControls") { return StringId::GsControls; }
        if (name == "GsDifficulty") { return StringId::GsDifficulty; }
        if (name == "GsCameraSway") { return StringId::GsCameraSway; }
        if (name == "GsCredits") { return StringId::GsCredits; }
        if (name == "GsMusic") { return StringId::GsMusic; }
        if (name == "GsSfx") { return StringId::GsSfx; }
        if (name == "GsKeyboard") { return StringId::GsKeyboard; }
        if (name == "GsMouse") { return StringId::GsMouse; }
        if (name == "GsJoystick") { return StringId::GsJoystick; }
        if (name == "GsGravisGrip") { return StringId::GsGravisGrip; }
        if (name == "GsGravisPad") { return StringId::GsGravisPad; }
        if (name == "GsSpaceOrb") { return StringId::GsSpaceOrb; }
        if (name == "GsVfx1") { return StringId::GsVfx1; }
        if (name == "GsDifficultyEasy") { return StringId::GsDifficultyEasy; }
        if (name == "GsDifficultyMedium") { return StringId::GsDifficultyMedium; }
        if (name == "GsDifficultyHard") { return StringId::GsDifficultyHard; }
        if (name == "GsOff") { return StringId::GsOff; }
        if (name == "GsOn") { return StringId::GsOn; }
        if (name == "GsRedefine") { return StringId::GsRedefine; }
        if (name == "GsCalibrate") { return StringId::GsCalibrate; }
        if (name == "GsMoveForwards") { return StringId::GsMoveForwards; }
        if (name == "GsMoveBackwards") { return StringId::GsMoveBackwards; }
        if (name == "GsTurnLeft") { return StringId::GsTurnLeft; }
        if (name == "GsTurnRight") { return StringId::GsTurnRight; }
        if (name == "GsStrafeLeft") { return StringId::GsStrafeLeft; }
        if (name == "GsStrafeRight") { return StringId::GsStrafeRight; }
        if (name == "GsStrafeModifier") { return StringId::GsStrafeModifier; }
        if (name == "GsFire1") { return StringId::GsFire1; }
        if (name == "GsFire2") { return StringId::GsFire2; }
        if (name == "GsDoUse") { return StringId::GsDoUse; }
        if (name == "GsNextWeapon") { return StringId::GsNextWeapon; }
        if (name == "GsLookUp") { return StringId::GsLookUp; }
        if (name == "GsLookDown") { return StringId::GsLookDown; }
        if (name == "GsTurnAround") { return StringId::GsTurnAround; }
        if (name == "GsLeftButton") { return StringId::GsLeftButton; }
        if (name == "GsRightButton") { return StringId::GsRightButton; }
        if (name == "GsMiddleButton") { return StringId::GsMiddleButton; }
        if (name == "GsBlank") { return StringId::GsBlank; }
        if (name == "GsAutoMapper") { return StringId::GsAutoMapper; }
        if (name == "GsShoulderLamp") { return StringId::GsShoulderLamp; }
        if (name == "GsWeapon9mmPistol") { return StringId::GsWeapon9mmPistol; }
        if (name == "GsWeaponShotgun") { return StringId::GsWeaponShotgun; }
        if (name == "GsWeaponFlamethrower") { return StringId::GsWeaponFlamethrower; }
        if (name == "GsWeaponPulseRifle") { return StringId::GsWeaponPulseRifle; }
        if (name == "GsWeaponSmartGun") { return StringId::GsWeaponSmartGun; }
        if (name == "GsBatteries") { return StringId::GsBatteries; }
        if (name == "GsMission") { return StringId::GsMission; }
        if (name == "GsOptionsMenu") { return StringId::GsOptionsMenu; }
        if (name == "GsSfxVolume") { return StringId::GsSfxVolume; }
        if (name == "GsCddaVolume") { return StringId::GsCddaVolume; }
        if (name == "GsExitGame") { return StringId::GsExitGame; }
        if (name == "GsNightVision") { return StringId::GsNightVision; }
        if (name == "GsNoAmmoAvailable") { return StringId::GsNoAmmoAvailable; }
        if (name == "GsRoundsAvailable") { return StringId::GsRoundsAvailable; }
        if (name == "GsAreYouSure") { return StringId::GsAreYouSure; }
        if (name == "GsYes") { return StringId::GsYes; }
        if (name == "GsNo") { return StringId::GsNo; }
        if (name == "GsSelected") { return StringId::GsSelected; }
        if (name == "GsAvailable") { return StringId::GsAvailable; }
        if (name == "GsNotAvailable") { return StringId::GsNotAvailable; }
        if (name == "GsDoorActivated") { return StringId::GsDoorActivated; }
        if (name == "GsDoorPoweredUp") { return StringId::GsDoorPoweredUp; }
        if (name == "GsSteamValveClosed") { return StringId::GsSteamValveClosed; }
        if (name == "GsFlameJetShutDown") { return StringId::GsFlameJetShutDown; }
        if (name == "GsLiftActivated") { return StringId::GsLiftActivated; }
        if (name == "GsBatteryRequired") { return StringId::GsBatteryRequired; }
        if (name == "GsJoyCentre") { return StringId::GsJoyCentre; }
        if (name == "GsJoyLowerRight") { return StringId::GsJoyLowerRight; }
        if (name == "GsFaceForward") { return StringId::GsFaceForward; }
        if (name == "GsLoadingDataStars") { return StringId::GsLoadingDataStars; }
        if (name == "GsLoadingData") { return StringId::GsLoadingData; }
        if (name == "GsHitAnyKey") { return StringId::GsHitAnyKey; }
        if (name == "GsIncomingTransfer") { return StringId::GsIncomingTransfer; }
        if (name == "GsMissionBrief") { return StringId::GsMissionBrief; }
        if (name == "GsPleaseWaitStarting") { return StringId::GsPleaseWaitStarting; }
        if (name == "GsStartMultiplayer") { return StringId::GsStartMultiplayer; }
        if (name == "GsJoinMultiplayer") { return StringId::GsJoinMultiplayer; }
        if (name == "GsMultiplayerOptions") { return StringId::GsMultiplayerOptions; }
        if (name == "GsStartGame") { return StringId::GsStartGame; }
        if (name == "GsMultiplayer") { return StringId::GsMultiplayer; }
        if (name == "GsLoadGame") { return StringId::GsLoadGame; }
        if (name == "GsContinueGame") { return StringId::GsContinueGame; }
        if (name == "GsSaveGame") { return StringId::GsSaveGame; }
        if (name == "GsQuitGame") { return StringId::GsQuitGame; }
        if (name == "GsMissionAssessment") { return StringId::GsMissionAssessment; }
        if (name == "GsAliens") { return StringId::GsAliens; }
        if (name == "GsSecrets") { return StringId::GsSecrets; }
        if (name == "GsMissionLabel") { return StringId::GsMissionLabel; }
        if (name == "GsPressAnyKey") { return StringId::GsPressAnyKey; }
        if (name == "GsWaitingForPlayers") { return StringId::GsWaitingForPlayers; }
        if (name == "GsScores") { return StringId::GsScores; }
        if (name == "GsGameSetup") { return StringId::GsGameSetup; }
        if (name == "GsNameOfGame") { return StringId::GsNameOfGame; }
        if (name == "GsStartAtLevel") { return StringId::GsStartAtLevel; }
        if (name == "GsMinimumGameLength") { return StringId::GsMinimumGameLength; }
        if (name == "GsAcidHell") { return StringId::GsAcidHell; }
        if (name == "GsSearchingNetGames") { return StringId::GsSearchingNetGames; }
        if (name == "GsOpen") { return StringId::GsOpen; }
        if (name == "GsClosed") { return StringId::GsClosed; }
        if (name == "GsBetweenLevels") { return StringId::GsBetweenLevels; }
        if (name == "GsLevel") { return StringId::GsLevel; }
        if (name == "GsPlayers") { return StringId::GsPlayers; }
        if (name == "GsStatus") { return StringId::GsStatus; }
        if (name == "GsEditYourData") { return StringId::GsEditYourData; }
        if (name == "GsYourName") { return StringId::GsYourName; }
        if (name == "GsFxMessage") { return StringId::GsFxMessage; }
        if (name == "GsInvalidSaveFile") { return StringId::GsInvalidSaveFile; }
        if (name == "GsErrorWritingSave") { return StringId::GsErrorWritingSave; }
        if (name == "GsUnused130") { return StringId::GsUnused130; }
        if (name == "GsInsertCd") { return StringId::GsInsertCd; }
        if (name == "GsNoPathTxt") { return StringId::GsNoPathTxt; }
        if (name == "GsDemo") { return StringId::GsDemo; }
        if (name == "GsRunMode") { return StringId::GsRunMode; }
        if (name == "GsBattery") { return StringId::GsBattery; }
        if (name == "GsReallyQuit") { return StringId::GsReallyQuit; }
        if (name == "GsPickup9mmAutomatic") { return StringId::GsPickup9mmAutomatic; }
        if (name == "GsPickupShotgun") { return StringId::GsPickupShotgun; }
        if (name == "GsPickupPulseRifle") { return StringId::GsPickupPulseRifle; }
        if (name == "GsPickupFlamethrower") { return StringId::GsPickupFlamethrower; }
        if (name == "GsPickupSmartGun") { return StringId::GsPickupSmartGun; }
        if (name == "GsPickupNotUsed") { return StringId::GsPickupNotUsed; }
        if (name == "GsPickupSeismicCharges") { return StringId::GsPickupSeismicCharges; }
        if (name == "GsPickupBattery") { return StringId::GsPickupBattery; }
        if (name == "GsPickupNightVision") { return StringId::GsPickupNightVision; }
        if (name == "GsPickup9mmClip") { return StringId::GsPickup9mmClip; }
        if (name == "GsPickupShotgunCartridges") { return StringId::GsPickupShotgunCartridges; }
        if (name == "GsPickupPulseClip") { return StringId::GsPickupPulseClip; }
        if (name == "GsPickupGrenades") { return StringId::GsPickupGrenades; }
        if (name == "GsPickupFlamethrowerFuel") { return StringId::GsPickupFlamethrowerFuel; }
        if (name == "GsPickupSmartgunAmmo") { return StringId::GsPickupSmartgunAmmo; }
        if (name == "GsPickupIdentityTag") { return StringId::GsPickupIdentityTag; }
        if (name == "GsPickupAutoMapper") { return StringId::GsPickupAutoMapper; }
        if (name == "GsPickupHypoPack") { return StringId::GsPickupHypoPack; }
        if (name == "GsPickupAcidVest") { return StringId::GsPickupAcidVest; }
        if (name == "GsPickupBodySuit") { return StringId::GsPickupBodySuit; }
        if (name == "GsPickupMediKit") { return StringId::GsPickupMediKit; }
        if (name == "GsPickupDermPatch") { return StringId::GsPickupDermPatch; }
        if (name == "GsPickupProtectiveBoots") { return StringId::GsPickupProtectiveBoots; }
        if (name == "GsPickupAdrenalineBurst") { return StringId::GsPickupAdrenalineBurst; }
        if (name == "GsPickupShoulderLamp") { return StringId::GsPickupShoulderLamp; }
        if (name == "GsNeed8Mb") { return StringId::GsNeed8Mb; }
        if (name == "GsFatalServerCrash") { return StringId::GsFatalServerCrash; }
        if (name == "GsPressReturnToContinue") { return StringId::GsPressReturnToContinue; }
        if (name == "GsPlayerQuit") { return StringId::GsPlayerQuit; }
        if (name == "GsPlayerJoined") { return StringId::GsPlayerJoined; }
        if (name == "GsPlayerSays") { return StringId::GsPlayerSays; }
        if (name == "GsNotInGame") { return StringId::GsNotInGame; }
        if (name == "GsKilledHimself") { return StringId::GsKilledHimself; }
        if (name == "GsYouKilled") { return StringId::GsYouKilled; }
        if (name == "GsKilledOther") { return StringId::GsKilledOther; }
        if (name == "GsPlayerCrashed") { return StringId::GsPlayerCrashed; }
        if (name == "GsExitSwitchActive") { return StringId::GsExitSwitchActive; }
        if (name == "GsThirtySeconds") { return StringId::GsThirtySeconds; }
        if (name == "GsSomeoneBeatYou") { return StringId::GsSomeoneBeatYou; }
        if (name == "GsSending") { return StringId::GsSending; }
        if (name == "GsSecondsToEvacuate") { return StringId::GsSecondsToEvacuate; }
        if (name == "GsKilledYou") { return StringId::GsKilledYou; }
        if (name == "GsYouWereKilled") { return StringId::GsYouWereKilled; }
        if (name == "GsYouKilledYourself") { return StringId::GsYouKilledYourself; }
        if (name == "GsLanguage") { return StringId::GsLanguage; }
        if (name == "GsLangEnglish") { return StringId::GsLangEnglish; }
        if (name == "GsLangFrench") { return StringId::GsLangFrench; }
        if (name == "GsLangItalian") { return StringId::GsLangItalian; }
        if (name == "GsLangSpanish") { return StringId::GsLangSpanish; }

        return std::nullopt;
    }
}
