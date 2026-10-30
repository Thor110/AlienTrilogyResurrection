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
        // The pause menu's second entry is owned while either of these is
        // carried (FUN_000432fc case 1).
        bool hasShoulderLamp = false;
        // Counts, as the original keeps them: shoulder lamps (DAT_000b0ad4,
        // pickup 0x19) and night-vision goggles (DAT_000b0ad8, pickup 8). Each
        // use spends one (FUN_00044b48 case 1).
        int shoulderLamps = 0;
        int nightVisionGoggles = 0;
        // Ticks left on the one that is lit: the lamp's DAT_000b0ad6 (0x708)
        // or the goggles' DAT_000b0ada (0xa8c), counted down by FUN_0003e698.
        int lampTicks = 0;
        int gogglesTicks = 0;
        static constexpr int LAMP_TICKS = 0x708;
        static constexpr int GOGGLES_TICKS = 0xa8c;

        // Using the entry (FUN_00044b48 case 1): nothing while one is still
        // lit; otherwise a lamp if there is one, else goggles. Returns 1 for
        // the lamp, 2 for the goggles, 0 for nothing.
        int UseLight()
        {
            if (lampTicks != 0 || gogglesTicks != 0) { return 0; }
            int lit = 0;
            if (shoulderLamps > 0) { --shoulderLamps; lampTicks = LAMP_TICKS; lit = 1; }
            else if (nightVisionGoggles > 0) { --nightVisionGoggles; gogglesTicks = GOGGLES_TICKS; lit = 2; }
            hasShoulderLamp = shoulderLamps > 0 || nightVisionGoggles > 0;
            return lit;
        }

        // One tick of FUN_0003e698. Returns true on the tick a light runs out.
        bool TickLight()
        {
            if (lampTicks > 0 && --lampTicks == 0) { return true; }
            if (gogglesTicks > 0 && --gogglesTicks == 0) { return true; }
            return false;
        }
        // A count, not a flag: battery-operated switches consume one each,
        // so more than one can be carried and running out matters.
        int batteries = 0;
        bool HasBatteries() const { return batteries > 0; }

        // Seismic charges (DAT_000b0ad0), from pickup 6 (0x3899f, no cap).
        // Fire 2 throws one with any weapon but the pulse rifle, whose Fire 2
        // is its grenade (FUN_0003efcc -> FUN_0003d794).
        int seismicCharges = 0;

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
            if (shoulderLamps < 1) { shoulderLamps = 1; }
            batteries = 9;
            seismicCharges = 99;
            // Equipped weapon deliberately left alone - the cheat gives things,
            // it does not change what the player is holding.
        }

        // THE WEAPON INDEX IS THE ORIGINAL'S INTERNAL SLOT (DAT_000b0aae):
        //     0 pistol   1 shotgun   2 pulse rifle   3 smartgun   4 flamethrower
        //
        // Not the order the select keys run in (1-5: pistol, shotgun, pulse
        // rifle, flamethrower, smartgun - see SlotForSelectKey). Every table in
        // the original is per slot, and slot 3 is the smartgun three times over:
        //   - FUN_000401d0's jump table (0x401bc) gives slot 3 the table at
        //     0x0ace88, whose sound events are 0901/0902smar, and slot 4 the one
        //     at 0x0ace70 (0501/0503flam);
        //   - FUN_0003d5b0 fires slot 3 through FUN_0002c1d4, which aims at up to
        //     three creatures, from DAT_000b0acc - the counter pickup 4, "Smart
        //     Gun", fills;
        //   - slot 4 fires FUN_0002c0a0's short, scattered stream from
        //     DAT_000b0aca, which pickup 3, "Flamethrower", fills.
        // The save/restore code converts between the two numberings with a 3/4
        // swap (0x17bd5).
        //
        // The port had 3 as the flamethrower: its graphics, names and pickups
        // used the key order while its firing used the slot order, so each of
        // the two weapons fired the other's rounds.
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
            case 3:  return &smartGun;
            case 4:  return &flamethrower;
            default: return nullptr;
            }
        }
        // The select keys, 1-5, in slot terms.
        static constexpr int SlotForSelectKey(int key)
        {
            constexpr int SLOTS[5] = { 0, 1, 2, 4, 3 };
            return (key >= 0 && key < 5) ? SLOTS[key] : 0;
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
        // THE ORIGINAL'S WEAPON LIST (0x1a7e28): one entry per slot, each with a
        // flag that pickups set and that is cleared when the weapon runs dry.
        // Selecting (FUN_00039004) and next-weapon (FUN_00038d26) both skip an
        // entry whose flag is clear - a weapon held but empty.
        bool Selectable(int i) const { return Available(i) && AmmoTotal(i) > 0; }

        // Next-weapon: forward from the current slot, wrapping (FUN_00038d26).
        int NextSelectable(int current) const
        {
            for (int step = 1; step < 5; ++step)
            {
                const int candidate = (current + step) % 5;
                if (Selectable(candidate)) { return candidate; }
            }
            return current;
        }

        // The automatic switch when the held weapon runs dry (FUN_00038e64):
        // the HIGHEST other slot that can still fire, so the best weapon first.
        // -1 if there is none - FUN_00038c78's "cannot switch".
        int BestOtherSelectable(int current) const
        {
            for (int candidate = 4; candidate >= 0; --candidate)
            {
                if (candidate != current && Selectable(candidate)) { return candidate; }
            }
            return -1;
        }

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
