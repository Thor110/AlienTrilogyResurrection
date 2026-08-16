#pragma once

#include <initializer_list>

namespace ALTEngine::Screens
{
    struct WeaponState
    {
        bool available = false; // false = "No ammo available" / not picked up
        bool equipped = false;  // shown bright regardless of cursor position, plus "Selected"
        int ammo = 0;

        // Only the pulse rifle uses this. The original keeps grenades in the
        // HIGH half of that weapon's own ammo word (DAT_000b0ac6), capped at
        // 0x14 by FUN_000387c0 - they belong to the weapon because the launcher
        // is underslung on it, not to an inventory slot of their own.
        int grenades = 0;
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

        // THE CANONICAL WEAPON ORDER, and it is the original's:
        //     0 pistol   1 shotgun   2 pulse rifle   3 flamethrower   4 smartgun
        //
        // Confirmed twice over. The select keys are 1-5 in that order (Edward,
        // 2026), and FUN_000401d0's switch reaches the per-weapon tables in that
        // sequence - the four-state table with the grenade launcher is the pulse
        // rifle, identified by its 0401gren sound cue.
        //
        // The pickup switch already used this order; the current-weapon code did
        // not, which is part of why a picked-up shotgun never became selectable.
        // Everything indexes through here now.
        WeaponState* ByIndex(int i)
        {
            switch (i)
            {
            case 0:  return &pistol;
            case 1:  return &shotgun;
            case 2:  return &pulseRifle;
            case 3:  return &flamethrower;
            case 4:  return &smartGun;
            default: return nullptr;
            }
        }
        const WeaponState* ByIndex(int i) const
        {
            return const_cast<PlayerInventoryState*>(this)->ByIndex(i);
        }

        bool Available(int i) const
        {
            const WeaponState* w = ByIndex(i);
            return w && w->available;
        }

        // Makes `i` the held weapon, if it can be held at all. Returns the index
        // actually equipped so the caller can keep its own state in step.
        int Equip(int i)
        {
            if (!Available(i)) { return -1; }
            for (int k = 0; k < 5; ++k)
            {
                if (WeaponState* w = ByIndex(k)) { w->equipped = (k == i); }
            }
            return i;
        }

        // The original's key 6: step to the next weapon that has been picked up,
        // wrapping. Returns the new index, or the old one if nothing else is
        // available.
        int NextAvailable(int current) const
        {
            for (int step = 1; step <= 5; ++step)
            {
                const int candidate = (current + step) % 5;
                if (Available(candidate)) { return candidate; }
            }
            return current;
        }

        WeaponState pistol{ true, true, 45 };
        WeaponState shotgun{};
        WeaponState flamethrower{};
        // Grenades ride with the pulse rifle rather than being their own item:
        // the original keeps them in the high half of that weapon's ammo word
        // (DAT_000b0ac6), because the launcher is underslung on it.
        WeaponState pulseRifle{};
        WeaponState smartGun{};
    };
}
