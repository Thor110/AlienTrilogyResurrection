#pragma once

#include <array>
#include <cstddef>
#include <initializer_list>

namespace ALTEngine::Screens
{
    struct WeaponState
    {
        bool available = false; // false = "No ammo available" / not picked up
        bool equipped = false;  // shown bright regardless of cursor position, plus "Selected"

        // THE ammunition, in the original's own two counters (see
        // PlayerHudState's note on FUN_0003aac8). `rounds` is what is loaded
        // and is what firing spends; `units` are spare magazines, which a
        // reload turns into `rounds`. Weapons without a magazine (shotgun,
        // smartgun) only ever use `rounds`.
        //
        // This is the only copy. The HUD, the pause menu and the weapon system
        // all read it here - a second, per-level copy in the HUD state drifted
        // from this one (pickups stopped adding ammo once this copy hit its
        // cap, because firing only ever drained the other).
        int rounds = 0;
        int units = 0;

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
        // would read as 999 anyway. Called every tick while the cheat is on, so
        // it writes the real counters and spent rounds really are replaced.
        void GiveEverything()
        {
            for (int i = 0; i < 5; ++i)
            {
                ByIndex(i)->available = true;
                SetAmmoTotal(i, 999);
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

        // Rounds per spare magazine, by canonical index. 0 = no magazine.
        static constexpr std::array<int, 5> ROUNDS_PER_UNIT{ 15, 0, 100, 100, 0 };

        // What the HUD and pause menu print: everything carried for a weapon.
        int AmmoTotal(int i) const
        {
            const WeaponState* w = ByIndex(i);
            if (!w) { return 0; }
            return w->units * ROUNDS_PER_UNIT[static_cast<size_t>(i)] + w->rounds;
        }

        // Adds rounds to a weapon's total, never taking it past `cap`. The new
        // rounds top up the loaded magazine and overflow into spare magazines,
        // exactly as the HUD's old per-frame pickup delta did.
        void AddAmmo(int i, int added, int cap)
        {
            WeaponState* w = ByIndex(i);
            if (!w || added <= 0) { return; }
            const int total = AmmoTotal(i);
            const int room = cap - total;
            if (room <= 0) { return; }
            SetAmmoTotal(i, total + (added < room ? added : room));
        }

        // Replaces a weapon's total. A total that divides exactly leaves the
        // last magazine LOADED rather than banked, so the weapon can fire at once.
        void SetAmmoTotal(int i, int total)
        {
            WeaponState* w = ByIndex(i);
            if (!w) { return; }
            if (total < 0) { total = 0; }
            const int magazine = ROUNDS_PER_UNIT[static_cast<size_t>(i)];
            if (magazine <= 0)
            {
                w->units = 0;
                w->rounds = total;
                return;
            }
            int units = total / magazine;
            int rounds = total % magazine;
            if (rounds == 0 && units > 0) { units--; rounds = magazine; }
            w->units = units;
            w->rounds = rounds;
        }

        // 9mm pistol with 45 rounds: one loaded magazine of 15 and two spares.
        WeaponState pistol{ true, true, 15, 2 };
        WeaponState shotgun{};
        WeaponState flamethrower{};
        // Grenades ride with the pulse rifle rather than being their own item:
        // the original keeps them in the high half of that weapon's ammo word
        // (DAT_000b0ac6), because the launcher is underslung on it.
        WeaponState pulseRifle{};
        WeaponState smartGun{};
    };
}
