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
        ExitGameTitle, // the pause menu's custom-drawn "EXIT GAME" panel label, all caps - distinct from the tree node's own "Exit Game" title-case label
        VSync,
        DisplayModeTitle,
        Windowed,
        Fullscreen,
        Borderless,
        MouseSensitivity,

        Count, // sentinel - not a real string, gives the tables their size
    };
}
