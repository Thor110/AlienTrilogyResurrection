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
        // 0 - Explosive Barrel
        // 1 - Single Crate
        // 2 - Double Crate
        // 3 - Switch Red Left Light
        // 4 - Switch Red Right Light
        // 5 - Switch Both Lights Off
        // 6 - Switch Both Lights Yellow
        // 7 - Large Switch Red Left Light
        // 8 - Large Switch Red Right Light
        // 9 - Large Switch Both Lights Off
        // 10 - Large Switch Both Lights Yellow
        // 11 - Switch Battery Red Left Light
        // 12 - Switch Battery Red Right Light
        // 13 - Switch Battery Both Lights Off
        // 14 - Switch Battery Both Lights Yellow
        // 15 - Large Switch Battery Red Left Light
        // 16 - Large Switch Battery Red Right Light
        // 17 - Large Switch Battery Both Lights Off
        // 18 - Large Switch Battery Both Lights Yellow
        // 19 - Boneship Switch Red Left Light
        // 20 - Boneship Switch Red Right Light
        // 21 - Boneship Switch Both Lights Off
        // 22 - Boneship Switch Both Lights Yellow
        // 23 - Boneship Switch Red Left Light
        // 24 - Boneship Switch Red Right Light
        // 25 - Boneship Switch Both Lights Off
        // 26 - Boneship Switch Both Lights Yellow
        // 27 - Boneship Switch Red Left Light
        // 28 - Boneship Switch Red Right Light
        // 29 - Boneship Switch Both Lights Off
        // 30 - Boneship Switch Both Lights Yellow
        // 31 - Boneship Switch Red Left Light
        // 32 - Boneship Switch Red Right Light
        // 33 - Boneship Switch Both Lights Off
        // 34 - Boneship Switch Both Lights Yellow
        // 35 - Steel Coil
        // 36 - Unused Shape
        // 37 - Pylon(Unused )
        // 38 - Computer(Unused? )
        // 39 - Egg Husk Shape Untextured
        // 40 - Stasis Pod Cover
        // 41 - Egg Husk
    }

    // PICKMOD.BND - pickup models (weapons, ammo, items)
    namespace PickMod
    {
        // 0 - Pistol
        // 1 - Shotgun
        // 2 - Pulse Rifle
        // 3 - Flamethrower
        // 4 - Smart Gun
        // 5 - Seismic Charge
        // 6 - Battery
        // 7 - Night Vision Goggles
        // 8 - Pistol Clip
        // 9 - Shotgun Shell
        // 10 - Pulse Rifle Clip
        // 11 - Pulse Rifle Grenade
        // 12 - Flamethrower Fuel
        // 13 - Smart Gun Ammunition
        // 14 - ID Badge
        // 15 - Auto Mapper
        // 16 - Hypo Pack
        // 17 - Acid Vest
        // 18 - Body Suit
        // 19 - Medi Kit
        // 20 - Dermpatch
        // 21 - Boots
        // 22 - Adrenaline Burst
        // 23 - Shoulder Lamp
        // 24 - Shotgun Ammunition
        // 25 - Pistol Shell
        // public Mesh menuJoystick, menuCamera;
    }

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
