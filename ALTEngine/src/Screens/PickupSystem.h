#pragma once

#include "PlayerHudState.h"
#include "PlayerInventoryState.h"

namespace ALTEngine::Screens
{
    // What every pickup type does, transcribed from FUN_000387c0's switch.
    //
    // The handler takes the type and two amounts - `param_2` and the register
    // the caller leaves in BX - and most weapon pickups add to BOTH a magazine
    // counter and a spare-unit counter.
    //
    // TYPE ORDER IS NOT WEAPON ORDER. This is the trap, and the port fell into
    // it: pickup 3 adds to DAT_000b0aca, which the HUD reads for weapon 4, the
    // SMARTGUN. Pickup 4 adds to DAT_000b0acc/ace, which is weapon 3, the
    // FLAMETHROWER. So the two are swapped relative to the weapon indices, and
    // picking up smartgun ammo was giving flamethrower ammo. Same swap on the
    // ammo-only variants 0x0d and 0x0e.
    //
    // The pairs are: every weapon has a "grant and fill" type and an "ammo only"
    // type. The first sets the weapon's availability bit in DAT_000b0ab4 and
    // resets its HUD flash; the second only adds ammo, and re-flashes the HUD
    // only if the weapon is already held.
    //
    //   grant  ammo-only  weapon         counters        caps
    //     0       9       pistol         abe / ac0       0x0f / 9
    //     1       0x0a    shotgun        ac2             100
    //     2       0x0b    pulse rifle    ac4 / ac6 low   100 / 9
    //     3       0x0d    SMARTGUN       aca             500   (0x0d adds a flat 100)
    //     4       0x0e    FLAMETHROWER   acc / ace low   100 / 9
    namespace Pickups
    {
        enum Type
        {
            PISTOL_AND_AMMO = 0,
            SHOTGUN_AND_AMMO = 1,
            PULSE_AND_AMMO = 2,
            SMARTGUN_AND_AMMO = 3,
            FLAMETHROWER_AND_AMMO = 4,

            PULSE_SPARE = 6,        // DAT_000b0ace high half - the non-pulse icon counter
            COUNTER_AD2 = 7,        // +1, untraced meaning
            COUNTER_AD8 = 8,        // +1, untraced meaning

            PISTOL_AMMO = 9,
            SHOTGUN_AMMO = 0x0a,
            PULSE_AMMO = 0x0b,
            GRENADES = 0x0c,        // caps at 0x14 - the pulse rifle's launcher
            SMARTGUN_AMMO = 0x0d,   // flat +100
            FLAMETHROWER_AMMO = 0x0e,
            COUNTER_AE2 = 0x0f,     // +1, untraced

            FLAG_80 = 0x10,         // raises bit 0x80 of DAT_000b0ab4

            // Health to at least 100 AND 900 ticks of terrain protection. One
            // pickup, two effects - the only type that does that.
            HEALTH_AND_SHIELD = 0x11,

            ARMOUR_SET_A = 0x12,    // DAT_000b0aba = amount - SET, not added
            ARMOUR_SET_B = 0x13,

            HEALTH_CAPPED = 0x14,   // + amount, clamped to 100
            HEALTH_UNCAPPED = 0x15, // + amount, NO clamp - can exceed 100
            TERRAIN_SPEED = 0x16,   // DAT_000b0abc += amount * 0x1e (30 ticks each)
            HEALTH_SET_200 = 0x17,  // health = 200 outright

            COUNTER_AD4 = 0x19,     // +1, untraced
        };

        // Caps, all from the handler.
        inline constexpr int PISTOL_MAG_CAP = 0x0f;
        inline constexpr int PISTOL_UNIT_CAP = 9;
        inline constexpr int SHOTGUN_CAP = 100;
        inline constexpr int MAG_CAP = 100;
        inline constexpr int UNIT_CAP = 9;
        inline constexpr int SMARTGUN_CAP = 500;
        inline constexpr int GRENADE_CAP = 0x14;
        inline constexpr int HEALTH_CAP = 100;
        inline constexpr int HEALTH_BOOST = 200;
        inline constexpr int SHIELD_TICKS = 900;
        inline constexpr int TERRAIN_SPEED_PER_UNIT = 0x1e;
        inline constexpr int SMARTGUN_AMMO_FLAT = 100;

        inline int Clamp(int value, int cap) { return value > cap ? cap : value; }

        // Applies a pickup. `amount` is the record's own amount times its
        // multiplier; `units` is the second value the original takes in BX,
        // which for weapon pickups is the number of spare magazines.
        //
        // Returns false for a type with no effect here, so the caller can log it
        // rather than silently swallow an unknown pickup.
        inline bool Apply(int type, int amount, int units,
                          PlayerInventoryState& inventory, PlayerHudState& hud)
        {
            auto grant = [&](int weaponIndex, int magCap, int unitCap) {
                if (WeaponState* w = inventory.ByIndex(weaponIndex))
                {
                    w->available = true;
                    w->ammo = Clamp(w->ammo + amount + units * magCap, magCap * (unitCap + 1));
                }
            };
            auto addAmmo = [&](int weaponIndex, int magCap, int unitCap) {
                if (WeaponState* w = inventory.ByIndex(weaponIndex))
                {
                    w->ammo = Clamp(w->ammo + amount + units * magCap, magCap * (unitCap + 1));
                }
            };

            switch (type)
            {
            // Weapon indices below are the CANONICAL ones - 2 pulse, 3
            // flamethrower, 4 smartgun - which is why 3 and 4 look crossed
            // against the pickup type numbers. See the note at the top.
            case PISTOL_AND_AMMO:       grant(0, PISTOL_MAG_CAP, PISTOL_UNIT_CAP); return true;
            case SHOTGUN_AND_AMMO:      grant(1, SHOTGUN_CAP, 0);                  return true;
            case PULSE_AND_AMMO:        grant(2, MAG_CAP, UNIT_CAP);               return true;
            case SMARTGUN_AND_AMMO:     grant(4, SMARTGUN_CAP, 0);                 return true;
            case FLAMETHROWER_AND_AMMO: grant(3, MAG_CAP, UNIT_CAP);               return true;

            case PISTOL_AMMO:       addAmmo(0, PISTOL_MAG_CAP, PISTOL_UNIT_CAP); return true;
            case SHOTGUN_AMMO:      addAmmo(1, SHOTGUN_CAP, 0);                  return true;
            case PULSE_AMMO:        addAmmo(2, MAG_CAP, UNIT_CAP);               return true;
            case FLAMETHROWER_AMMO: addAmmo(3, MAG_CAP, UNIT_CAP);               return true;

            case SMARTGUN_AMMO:
                // A flat 100, not the record's amount.
                if (WeaponState* w = inventory.ByIndex(4))
                {
                    w->ammo = Clamp(w->ammo + SMARTGUN_AMMO_FLAT, SMARTGUN_CAP);
                }
                return true;

            case GRENADES:
                inventory.pulseRifle.grenades = Clamp(inventory.pulseRifle.grenades + amount, GRENADE_CAP);
                return true;

            // THE DERM PATCH and its relatives. These were falling through to
            // the default and doing nothing at all.
            case HEALTH_CAPPED:
                hud.health = static_cast<int16_t>(Clamp(hud.health + amount, HEALTH_CAP));
                return true;
            case HEALTH_UNCAPPED:
                // Deliberately unclamped - the original adds and does not check.
                hud.health = static_cast<int16_t>(hud.health + amount);
                return true;
            case HEALTH_SET_200:
                hud.health = HEALTH_BOOST;
                return true;
            case HEALTH_AND_SHIELD:
                if (hud.health < HEALTH_CAP) { hud.health = HEALTH_CAP; }
                hud.terrainShield = SHIELD_TICKS;
                return true;

            case ARMOUR_SET_A:
            case ARMOUR_SET_B:
                // SET, not added.
                hud.armourItem = static_cast<int16_t>(amount);
                return true;

            case TERRAIN_SPEED:
                hud.terrainSpeed = static_cast<int16_t>(hud.terrainSpeed + amount * TERRAIN_SPEED_PER_UNIT);
                return true;

            case PULSE_SPARE:
                inventory.pulseRifle.grenades = Clamp(inventory.pulseRifle.grenades + amount, GRENADE_CAP);
                return true;

            case FLAG_80:
                inventory.hasAutoMapper = true;   // best reading of DAT_000b0ab4 bit 0x80
                return true;
            case COUNTER_AD4:
                inventory.hasShoulderLamp = true; // best reading of DAT_000b0ad4
                return true;
            case COUNTER_AD2:
                inventory.batteries += (amount > 0 ? amount : 1);
                return true;

            // Counters whose meaning is not traced. Counted so nothing is lost.
            case COUNTER_AD8:
            case COUNTER_AE2:
                return true;

            default:
                return false;
            }
        }
    }
}
