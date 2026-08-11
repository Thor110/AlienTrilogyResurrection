#pragma once

#include <initializer_list>

namespace ALTEngine::Screens
{
    struct WeaponState
    {
        bool available = false; // false = "No ammo available" / not picked up
        bool equipped = false;  // shown bright regardless of cursor position, plus "Selected"
        int ammo = 0;
    };

    // Placeholder inventory/equipment state - there's no real gameplay
    // or save/load system yet to query this from, so PauseMenuScreen
    // just takes one of these directly rather than reaching into
    // anything real. Default-constructed to match the reference
    // screenshots exactly (9mm Pistol equipped with 45 rounds,
    // everything else unavailable) so the rendering can be built and
    // tested against a concrete, realistic state - swap this for a real
    // query into player state once one exists; nothing else about
    // PauseMenuScreen should need to change.
    struct PlayerInventoryState
    {
        bool hasAutoMapper = false;
        bool hasShoulderLamp = false;
        // A count, not a flag: battery-operated switches consume one each,
        // so more than one can be carried and running out matters.
        int batteries = 0;
        bool HasBatteries() const { return batteries > 0; }

        // "Fully Loaded" cheat: every weapon and item available with full ammo.
        //
        // 999 is the original's own display clamp (its number routines clamp to
        // 0..999), so it is the most ammo the HUD can show - anything higher
        // would read as 999 anyway.
        void GiveEverything()
        {
            for (WeaponState* w : { &pistol, &shotgun, &flamethrower, &pulseRifle, &smartGun })
            {
                w->available = true;
                w->ammo = 999;
            }
            hasAutoMapper = true;
            hasShoulderLamp = true;
            batteries = 9;
            // Equipped weapon deliberately left alone - the cheat gives things,
            // it does not change what the player is holding.
        }

        WeaponState pistol{ true, true, 45 };
        WeaponState shotgun{};
        WeaponState flamethrower{};
        WeaponState pulseRifle{};
        WeaponState smartGun{};
    };
}
