#pragma once

// Confirmed mesh index catalogs for the game's three model-container
// files (OBJ3D.BND, PICKMOD.BND, OPTOBJ.BND) - salvaged from Edward's
// prior reverse-engineering work (2026), not derived/guessed.
//
// Not wired to any loader yet - no OBJ3D/PICKMOD/OPTOBJ loader exists in
// ALTEngine at all yet, since the 3D rendering pipeline (SDL GPU API)
// hasn't been built. This is purely a reference for when one is.
//
// OPTOBJ specifically is already in active use - see MenuTree.cpp's
// ModelIndex namespace, which mirrors the OPTOBJ list below exactly.
// Keep both in sync if either changes.
//
// Verbatim from the original source (comments.txt), including its own
// uncertainty markers (e.g. "Multitap?", "(Unused?)") - preserved as-is
// rather than resolved, since resolving them isn't something to guess at.

namespace ALTEngine::Formats::ModelIndices
{
    // OBJ3D.BND - in-game world objects (crates, switches, etc)
    namespace Obj3D
    {
        constexpr int ExplosiveBarrel = 0;
        constexpr int SingleCrate = 1;
        constexpr int DoubleCrate = 2;
        constexpr int SwitchRedLeftLight = 3;
        constexpr int SwitchRedRightLight = 4;
        constexpr int SwitchBothLightsOff = 5;
        constexpr int SwitchBothLightsYellow = 6;
        constexpr int LargeSwitchRedLeftLight = 7;
        constexpr int LargeSwitchRedRightLight = 8;
        constexpr int LargeSwitchBothLightsOff = 9;
        constexpr int LargeSwitchBothLightsYellow = 10;
        constexpr int SwitchBatteryRedLeftLight = 11;
        constexpr int SwitchBatteryRedRightLight = 12;
        constexpr int SwitchBatteryBothLightsOff = 13;
        constexpr int SwitchBatteryBothLightsYellow = 14;
        constexpr int LargeSwitchBatteryRedLeftLight = 15;
        constexpr int LargeSwitchBatteryRedRightLight = 16;
        constexpr int LargeSwitchBatteryBothLightsOff = 17;
        constexpr int LargeSwitchBatteryBothLightsYellow = 18;
        constexpr int BoneshipSwitchRedLeftLight1 = 19;
        constexpr int BoneshipSwitchRedRightLight1 = 20;
        constexpr int BoneshipSwitchBothLightsOff1 = 21;
        constexpr int BoneshipSwitchBothLightsYellow1 = 22;
        constexpr int BoneshipSwitchRedLeftLight2 = 23;
        constexpr int BoneshipSwitchRedRightLight2 = 24;
        constexpr int BoneshipSwitchBothLightsOff2 = 25;
        constexpr int BoneshipSwitchBothLightsYellow2 = 26;
        constexpr int BoneshipSwitchRedLeftLight3 = 27;
        constexpr int BoneshipSwitchRedRightLight3 = 28;
        constexpr int BoneshipSwitchBothLightsOff3 = 29;
        constexpr int BoneshipSwitchBothLightsYellow3 = 30;
        constexpr int BoneshipSwitchRedLeftLight4 = 31;
        constexpr int BoneshipSwitchRedRightLight4 = 32;
        constexpr int BoneshipSwitchBothLightsOff4 = 33;
        constexpr int BoneshipSwitchBothLightsYellow4 = 34;
        constexpr int SteelCoil = 35;
        constexpr int UnusedShape = 36;
        constexpr int Pylon = 37;         // unused
        constexpr int Computer = 38;      // unused
        constexpr int EggHuskUntextured = 39;
        constexpr int StasisPodCover = 40;
        constexpr int EggHusk = 41;
    }

    // PICKMOD.BND indices - CORRECTED (Edward, 2026): the original
    // comments.txt list numbered these as a contiguous 0-25 range, but
    // that was off by one/two - indices 5 and 24 are genuinely NULL
    // (nonexistent) slots, not just omitted from the list, and every
    // index after each NULL shifts accordingly. This exactly matches
    // the real PICKMOD.BND's own section numbering, confirmed earlier:
    // 26 real sections, gaps at M005 and M024. The OLD numbering below
    // wasn't a crash risk (every old index still pointed at a real,
    // existing section) - it silently loaded the WRONG model for
    // everything past each gap (e.g. old Battery=6 actually loaded
    // M006, which is really Seismic Charge) - this is what "a few
    // textures are wrong and a few models are in the wrong place" was.
    namespace PickMod
    {
        constexpr int Pistol = 0;
        constexpr int Shotgun = 1;
        constexpr int PulseRifle = 2;
        constexpr int Flamethrower = 3;
        constexpr int SmartGun = 4;
        // 5 - NULL (confirmed missing M005)
        constexpr int SeismicCharge = 6;
        constexpr int Battery = 7;
        constexpr int NightVisionGoggles = 8;
        constexpr int PistolClip = 9;
        constexpr int ShotgunShell = 10;
        constexpr int PulseRifleClip = 11;
        constexpr int PulseRifleGrenade = 12;
        constexpr int FlamethrowerFuel = 13;
        constexpr int SmartGunAmmunition = 14;
        constexpr int IdBadge = 15;
        constexpr int AutoMapper = 16;
        constexpr int HypoPack = 17;
        constexpr int AcidVest = 18;
        constexpr int BodySuit = 19;
        constexpr int MediKit = 20;
        constexpr int Dermpatch = 21;
        constexpr int Boots = 22;
        constexpr int AdrenalineBurst = 23;
        // 24 - NULL (confirmed missing M024)
        constexpr int ShoulderLamp = 25;
        constexpr int ShotgunAmmunition = 26;
        constexpr int PistolShell = 27;
    }
    // Ammo-type pairings (which ammo model goes with which weapon, for
    // the pause menu's two-model weapon display) are a reasonable guess
    // from the reference screenshots' silhouettes, not confirmed:
    // Pistol->PistolClip (the screenshot actually shows a bar-shaped
    // gauge, not a shell/clip silhouette - least confident pairing here),
    // Shotgun->ShotgunShell (screenshot shows a red shell - good match),
    // PulseRifle->PulseRifleClip, Flamethrower->FlamethrowerFuel,
    // SmartGun->SmartGunAmmunition (all plausible silhouette matches,
    // none confirmed).

    // OPTOBJ.BND - menu models (confirmed - mirrors MenuTree.cpp's
    // ModelIndex namespace exactly; that's the version with actual named
    // constants in active use, this is just the reference list)
    //
    // Bonus: ModelRenderer.cs's own header-byte comment independently
    // lists these same 14 entries in the same order (cross-confirms the
    // index ordering from a second source), and additionally gives the
    // raw 4-byte identifier value for each - included below since it's
    // a second, independent way to identify a model (by its actual
    // header bytes, not just its section index/order).
    //
    // CONFIRMED against the real OPTOBJ.BND (Edward, 2026): all 14
    // sections' identifier bytes match this list exactly, in exact
    // section order (M000-M013 = index 0-13).
    //
    // Indices 2/3/13's real-world device names are the actual original
    // button text (SpaceOrb 360, Gravis Grip/Gravis Pad, VFX-1) - the
    // "Gamepad"/"Multitap"/"Headphones" labels below were Edward's
    // visual-impression names from looking at the models, not the
    // devices' actual identities.
    namespace OptObj
    {
        // 0  - Joystick               (identifier FC 56 5A 00)
        // 1  - Camera                 (identifier 8C 47 5A 00)
        // 2  - Gamepad                (identifier 5C 94 02 83) - real device: SpaceOrb 360 (confirmed original button text)
        // 3  - Multitap?              (identifier 98 66 5A 00) - real devices: Gravis Grip AND Gravis Pad (confirmed original button text) - both use this index
        // 4  - Hard Drive Saving <-   (identifier A4 59 5A 00)
        // 5  - Hard Drive Loading -> (identifier 90 59 5A 00)
        // 6  - Camera Crossed Out     (identifier 4C 67 5A 00)
        // 7  - Keyboard               (identifier 34 66 5A 00)
        // 8  - Mouse                  (identifier 88 67 5A 00)
        // 9  - Computer, Monitor and Keyboard (identifier 14 68 5A 00)
        // 10 - Two Linked Computers, Monitors and Keyboards ( Multiplayer ) (identifier 00 72 5A 00)
        // 11 - Speaker ( Disc Music ) (identifier B0 67 5A 00)
        // 12 - Speaker ( Sound Effects ) (identifier 68 48 5A 00)
        // 13 - Headphones             (identifier 9C 67 5A 00) - real device: VFX-1 (confirmed original button text) - VFX-1 was actually a VR headset
    }
}
