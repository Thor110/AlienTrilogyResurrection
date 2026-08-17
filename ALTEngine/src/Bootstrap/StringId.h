#pragma once

namespace ALTEngine::Bootstrap
{
    // One entry per distinct translatable UI string. Deliberately NOT
    // used as an internal identifier anywhere - MenuNode::label stays
    // the stable, English-based key that ApplyLeafAction/
    // FindChildIndexByLabel/parentLabel comparisons already rely on
    // throughout the codebase (Options > Quality's "if (parentLabel ==
    // "Quality")", the pause menu's "Exit Game"/"Yes"/"No" checks,
    // etc) - localizing that field directly would silently break every
    // one of those comparisons the moment a non-English language was
    // selected. StringId is purely "what to actually draw on screen",
    // looked up alongside label rather than replacing it (Edward, 2026:
    // "lay the foundations for a language system").
    //
    // Ordering matters for automation - a translation script filling in
    // a new language's table just needs to produce values in this same
    // order. Add new entries at the END only, never reorder/remove
    // existing ones, so old translated tables don't silently shift.
    enum class StringId
    {
        // Main Menu
        StartGame,
        Multiplayer,
        LoadGame,

        // Options top level
        Options,
        Volume,
        Music,
        Sfx,
        Controls,
        Keyboard,
        Mouse,
        Joystick,
        GravisGrip,
        GravisPad,
        SpaceOrb360,
        Vfx1,
        Difficulty,
        AcidReign,
        RagingTerror,
        Xenomania,
        CameraSway,
        Off,
        On,
        Graphics,
        Quality,
        Original,
        Smoothed,
        Resolution,
        LanguageMenuTitle, // the "Language" submenu entry itself - distinct from any specific language's own name below
        Credits,

        // Language names - these name themselves (e.g. "Français" is
        // French's own entry in every language's table, since a language
        // selector conventionally shows each option in its own language,
        // not translated into whichever language is currently active).
        LanguageEnglish,
        LanguageFrench,
        LanguageItalian,
        LanguageSpanish,
        LanguageGerman,
        LanguageJapanese,

        // Redefine controls
        Redefine,
        RestoreDefaults,
        AreYouSure,
        Yes,
        No,

        // Redefine controls - action names (Bootstrap/InputActions.h's
        // ActionLabel), shown as "{name}: {binding}" in the Redefine list
        ActionMoveForward,
        ActionMoveBackward,
        ActionStrafeLeft,
        ActionStrafeRight,
        ActionTurnLeft,
        ActionTurnRight,
        ActionFire1,
        ActionFire2,
        ActionUse,
        ActionStrafeModifier,
        ActionRunMode,
        ActionRunModifier,
        ActionSelectWeapon1,
        ActionSelectWeapon2,
        ActionSelectWeapon3,
        ActionSelectWeapon4,
        ActionSelectWeapon5,
        ActionNextWeapon,
        ActionTurnaround,
        ActionWeaponSelectMenu,
        ActionPause,

        // Pause menu - weapon/equipment list
        AutoMapper,
        ShoulderLamp,
        Pistol9mm,
        Shotgun,
        Flamethrower,
        PulseRifle,
        SmartGun,
        Batteries,
        Mission,
        SaveGame,
        ExitGame,
        SfxVolume,
        MusicVolume,

        // Standalone menu chrome
        PressEscToGoBack,
        PressEnterToSelect,
        CreditsTitle,
        CreditsPlaceholder,
        PressAKeyToBind,
        PressAMouseButtonOrWheelToBind,
        OrEscToCancel,
        OptionsTitle, // the Options SCREEN's own title ("OPTIONS", all caps) - distinct from the menu label ("Options", title case) used elsewhere, since the original code had these as two separate strings
        NotAvailable, // pause menu's weapon/ammo status text ("Not available")
        Available,    // shown under an item the player HAS
        Selected,     // shown under the equipped weapon
        ExitGameTitle, // the pause menu's custom-drawn "EXIT GAME" panel label, all caps - distinct from the tree node's own "Exit Game" title-case label
        VSync,
        DisplayModeTitle,
        Windowed,
        Fullscreen,
        Borderless,
        MouseSensitivity,
        Modern,
        EnableAll,
        AutomaticDoors,
        Custom,
        KeepItems,
        SkipEndLevelScreen,
        PlayerJumping,
        StunnedEnemies,
        DescEnableAll,
        DescAutomaticDoors,
        DescKeepItems,
        DescSkipEndLevel,
        DescPlayerJumping,
        DescStunnedEnemies,
        FreeLook,
        DescFreeLook,
        RenderDistance,
        DescRenderDistance,
        LevelSelect,
        DescLevelSelect,
        LiveMinimap,
        DescLiveMinimap,
        PresetConvenience,
        DescPresetConvenience,
        PresetModernised,
        DescPresetModernised,
        PresetTesting,
        DescPresetTesting,
        EnableCheats,
        DescEnableCheats,
        Cheats,
        FullyLoaded,
        DescFullyLoaded,
        MaximumHealth,
        DescMaximumHealth,


        // The original game's own text, extracted from LANGUAGE/*.BIN. The
        // number is that file's ordinal index, which is its only identifier -
        // the files have no header or key table.
        GsOptions,   // 0
        GsRedefineKeyboard,   // 1
        GsRedefineMouse,   // 2
        GsPressEscToGoBack,   // 3
        GsPressEnterToAdjust,   // 4
        GsPressEnterToSelect,   // 5
        GsDefaultPlayerName,   // 6
        GsCheatInvincible,   // 7
        GsCheatAllWeapons,   // 8
        GsCheatAllAmmo,   // 9
        GsCheatGotoLevel,   // 10
        GsCheatMaxLevel,   // 11
        GsCheatSetEverything,   // 12
        GsCheatWrongGame,   // 13
        GsCheatDoomed,   // 14
        GsCheatYouWish,   // 15
        GsCheatLookingCool,   // 16
        GsCheatDuking,   // 17
        GsCheatThatsYou,   // 18
        GsCheatNiceTry,   // 19
        GsCheatOi,   // 20
        GsVolume,   // 21
        GsControls,   // 22
        GsDifficulty,   // 23
        GsCameraSway,   // 24
        GsCredits,   // 25
        GsMusic,   // 26
        GsSfx,   // 27
        GsKeyboard,   // 28
        GsMouse,   // 29
        GsJoystick,   // 30
        GsGravisGrip,   // 31
        GsGravisPad,   // 32
        GsSpaceOrb,   // 33
        GsVfx1,   // 34
        GsDifficultyEasy,   // 35
        GsDifficultyMedium,   // 36
        GsDifficultyHard,   // 37
        GsOff,   // 38
        GsOn,   // 39
        GsRedefine,   // 40
        GsCalibrate,   // 41
        GsMoveForwards,   // 42
        GsMoveBackwards,   // 43
        GsTurnLeft,   // 44
        GsTurnRight,   // 45
        GsStrafeLeft,   // 46
        GsStrafeRight,   // 47
        GsStrafeModifier,   // 48
        GsFire1,   // 49
        GsFire2,   // 50
        GsDoUse,   // 51
        GsNextWeapon,   // 52
        GsLookUp,   // 53
        GsLookDown,   // 54
        GsTurnAround,   // 55
        GsLeftButton,   // 56
        GsRightButton,   // 57
        GsMiddleButton,   // 58
        GsBlank,   // 59
        GsAutoMapper,   // 60
        GsShoulderLamp,   // 61
        GsWeapon9mmPistol,   // 62
        GsWeaponShotgun,   // 63
        GsWeaponFlamethrower,   // 64
        GsWeaponPulseRifle,   // 65
        GsWeaponSmartGun,   // 66
        GsBatteries,   // 67
        GsMission,   // 68
        GsOptionsMenu,   // 69
        GsSfxVolume,   // 70
        GsCddaVolume,   // 71
        GsExitGame,   // 72
        GsNightVision,   // 73
        GsNoAmmoAvailable,   // 74
        GsRoundsAvailable,   // 75
        GsAreYouSure,   // 76
        GsYes,   // 77
        GsNo,   // 78
        GsSelected,   // 79
        GsAvailable,   // 80
        GsNotAvailable,   // 81
        GsDoorActivated,   // 82
        GsDoorPoweredUp,   // 83
        GsSteamValveClosed,   // 84
        GsFlameJetShutDown,   // 85
        GsLiftActivated,   // 86
        GsBatteryRequired,   // 87
        GsJoyCentre,   // 88
        GsJoyLowerRight,   // 89
        GsFaceForward,   // 90
        GsLoadingDataStars,   // 91
        GsLoadingData,   // 92
        GsHitAnyKey,   // 93
        GsIncomingTransfer,   // 94
        GsMissionBrief,   // 95
        GsPleaseWaitStarting,   // 96
        GsStartMultiplayer,   // 97
        GsJoinMultiplayer,   // 98
        GsMultiplayerOptions,   // 99
        GsStartGame,   // 100
        GsMultiplayer,   // 101
        GsLoadGame,   // 102
        GsContinueGame,   // 103
        GsSaveGame,   // 104
        GsQuitGame,   // 105
        GsMissionAssessment,   // 106
        GsAliens,   // 107
        GsSecrets,   // 108
        GsMissionLabel,   // 109
        GsPressAnyKey,   // 110
        GsWaitingForPlayers,   // 111
        GsScores,   // 112
        GsGameSetup,   // 113
        GsNameOfGame,   // 114
        GsStartAtLevel,   // 115
        GsMinimumGameLength,   // 116
        GsAcidHell,   // 117
        GsSearchingNetGames,   // 118
        GsOpen,   // 119
        GsClosed,   // 120
        GsBetweenLevels,   // 121
        GsLevel,   // 122
        GsPlayers,   // 123
        GsStatus,   // 124
        GsEditYourData,   // 125
        GsYourName,   // 126
        GsFxMessage,   // 127
        GsInvalidSaveFile,   // 128
        GsErrorWritingSave,   // 129
        GsUnused130,   // 130
        GsInsertCd,   // 131
        GsNoPathTxt,   // 132
        GsDemo,   // 133
        GsRunMode,   // 134
        GsBattery,   // 135
        GsReallyQuit,   // 136
        GsPickup9mmAutomatic,   // 137
        GsPickupShotgun,   // 138
        GsPickupPulseRifle,   // 139
        GsPickupFlamethrower,   // 140
        GsPickupSmartGun,   // 141
        GsPickupNotUsed,   // 142
        GsPickupSeismicCharges,   // 143
        GsPickupBattery,   // 144
        GsPickupNightVision,   // 145
        GsPickup9mmClip,   // 146
        GsPickupShotgunCartridges,   // 147
        GsPickupPulseClip,   // 148
        GsPickupGrenades,   // 149
        GsPickupFlamethrowerFuel,   // 150
        GsPickupSmartgunAmmo,   // 151
        GsPickupIdentityTag,   // 152
        GsPickupAutoMapper,   // 153
        GsPickupHypoPack,   // 154
        GsPickupAcidVest,   // 155
        GsPickupBodySuit,   // 156
        GsPickupMediKit,   // 157
        GsPickupDermPatch,   // 158
        GsPickupProtectiveBoots,   // 159
        GsPickupAdrenalineBurst,   // 160
        GsPickupShoulderLamp,   // 161
        GsNeed8Mb,   // 162
        GsFatalServerCrash,   // 163
        GsPressReturnToContinue,   // 164
        GsPlayerQuit,   // 165
        GsPlayerJoined,   // 166
        GsPlayerSays,   // 167
        GsNotInGame,   // 168
        GsKilledHimself,   // 169
        GsYouKilled,   // 170
        GsKilledOther,   // 171
        GsPlayerCrashed,   // 172
        GsExitSwitchActive,   // 173
        GsThirtySeconds,   // 174
        GsSomeoneBeatYou,   // 175
        GsSending,   // 176
        GsSecondsToEvacuate,   // 177
        GsKilledYou,   // 178
        GsYouWereKilled,   // 179
        GsYouKilledYourself,   // 180
        GsLanguage,   // 181
        GsLangEnglish,   // 182
        GsLangFrench,   // 183
        GsLangItalian,   // 184
        GsLangSpanish,   // 185

        Count, // sentinel - not a real string, gives the tables their size
    };
}
