#pragma once

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
        bool hasBatteries = false;

        WeaponState pistol{ true, true, 45 };
        WeaponState shotgun{};
        WeaponState flamethrower{};
        WeaponState pulseRifle{};
        WeaponState smartGun{};
    };
}
