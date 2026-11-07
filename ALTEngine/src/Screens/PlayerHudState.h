#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace ALTEngine::Screens
{
    // The player's health, armour and ammunition, laid out the way the original
    // stores them so the HUD can display the same numbers.
    //
    // Everything here is transcribed from FUN_0003e4a8 (damage) and
    // FUN_0003aac8 (the HUD's own ammo display) - see the long note in
    // Renderer/HudPanel.h for the addresses each field corresponds to.
    struct PlayerHudState
    {
        static constexpr int WEAPON_COUNT = 5;

        // 0x000b0ab8. (Armour, 0x000b0aba, is armourItem below - it used to be
        // here a second time as well, and the copy damage read was one nothing
        // ever set, so armour pickups protected against nothing.)
        int16_t health = 100;

        // 0x000b0aae. Which weapon's ammo the HUD shows.
        int16_t currentWeapon = 0;

        // Ammo is NOT one counter per weapon in the original. Two weapons
        // store a unit count plus a remainder and the HUD adds them together;
        // the other three are a single counter:
        //
        //   weapon 0: units x 15  + remainder   (0x000b0ac0, 0x000b0abe)
        //   weapon 1: remainder only            (0x000b0ac2)
        //   weapon 2: units x 100 + remainder   (0x000b0ac6, 0x000b0ac4)
        //   weapon 3: units x 100 + remainder   (0x000b0ace, 0x000b0acc)
        //   weapon 4: remainder only            (0x000b0aca)
        //
        // Those counters live in PlayerInventoryState (WeaponState::rounds and
        // ::units), which persists across levels. This is only the number the
        // HUD prints, refreshed from the inventory every frame.
        int currentAmmoDisplay = 0;

        // 0x000b0b30. Ticks of invulnerability after a hit - while non-zero the
        // original skips its whole damage path.
        int16_t damageCooldown = 0;

        // The carried-item counters the HUD's icon stack reads. Each icon is
        // drawn only when its counter is non-zero (FUN_0003aac8), which is why
        // they come and go rather than sitting there greyed out.
        //
        // Grenades are DAT_000b0ac6's high half, capped at 0x14 (20) by
        // FUN_000387c0, and their icon is the one tied to the PULSE RIFLE being
        // in hand - the launcher is underslung on it.
        static constexpr int GRENADE_MAX = 0x14;
        int16_t grenades = 0;
        int16_t seismicCharges = 0;   // shown with any weapon but the pulse rifle

        // The other three, identified by WHAT READS THEM rather than by any
        // name in the binary. None of them is the wall charges.
        //
        // DAT_000b0aba - read and written inside the player damage function
        //   FUN_0003e4a8, and granted by the pickup handler. It absorbs damage:
        //   ARMOUR. Not a timer; it is spent, not counted down.
        //
        // DAT_000b0abc - gates the terrain slowdown. FUN_0003d2b8 and
        //   FUN_0003d340 only halve movement on cell attributes 5 and 9 when
        //   this is ZERO, so while it is running the player moves at full speed
        //   through whatever those cells are. Counted down once per tick by
        //   FUN_0003e698.
        //
        // DAT_000b0ae4 - written by the terrain handler FUN_0003dff0 and read by
        //   the damage function. Protection against damaging floors, and also on
        //   the per-tick countdown.
        //
        // The last two are timers, which is why their icons appear for a while
        // and then go.
        int16_t armourItem = 0;      // DAT_000b0aba
        int16_t terrainSpeed = 0;    // DAT_000b0abc, ticks remaining
        int16_t terrainShield = 0;   // DAT_000b0ae4, ticks remaining

        // FUN_0003e698 decrements the timed ones once per logic tick.
        void TickCarriedItems()
        {
            if (terrainSpeed > 0) { terrainSpeed = static_cast<int16_t>(terrainSpeed - 1); }
            if (terrainShield > 0) { terrainShield = static_cast<int16_t>(terrainShield - 1); }
        }

        bool dead = false; // DAT_000b0cc0 bit 0x20

        int CurrentAmmoTotal() const { return currentAmmoDisplay; }

        // FUN_0003e4a8, which EVERY source of player damage goes through - enemy
        // attacks (cause 4), barrel blasts, acid blood and burning floors
        // (cause 5) alike:
        //
        //   - nothing at all while the damage cooldown (DAT_000b0b30) or the
        //     terrain shield (DAT_000b0ae4) is running;
        //   - otherwise armour (DAT_000b0aba) takes the WHOLE hit if there is
        //     any - it does not spill over into health - and only once it is
        //     gone does health (DAT_000b0ab8) drop, dying below 1;
        //   - either way the cooldown restarts at 0x1e.
        //
        // Returns true if the hit landed. `cause` picks the death clip.
        static constexpr int DAMAGE_COOLDOWN = 0x1e;
        // In any network game, co-operative included, it is 8 (0x3e57f).
        static constexpr int DAMAGE_COOLDOWN_NETWORK = 8;
        bool networkGame = false;
        int DamageCooldown() const { return networkGame ? DAMAGE_COOLDOWN_NETWORK : DAMAGE_COOLDOWN; }
        // The sound a landed hit makes, for the caller to play and clear
        // (FUN_0003e4a8, 0x3e4e0-0x3e59a, through FUN_00040ba0): 0202ripl (3)
        // for a hit off health, the killing one included; 5003metl (0x24) for
        // a cause-3 hit taken by armour; nothing else. -1 for none.
        static constexpr int HURT_SOUND = 3;
        static constexpr int ARMOUR_STRUCK_SOUND = 0x24;
        int pendingSound = -1;
        // OURS, co-operative: a player who has reached the exit waits there
        // for the others and takes no harm meanwhile.
        bool waitingAtExit = false;
        int lastDamageCause = -1;
        // Counts the hits that came off health, for the remark each draws
        // (FUN_0003e618 runs on that path only, not armour's).
        int healthHits = 0;
        bool ApplyDamage(int amount, int cause)
        {
            if (damageCooldown > 0 || terrainShield > 0 || waitingAtExit) { return false; }
            lastDamageCause = cause;
            pendingSound = (armourItem > 0 && cause == 3) ? ARMOUR_STRUCK_SOUND : -1;
            if (armourItem == 0)
            {
                pendingSound = HURT_SOUND;
                healthHits++;
                health = static_cast<int16_t>(health - amount);
                if (health < 1)
                {
                    health = 0;
                    dead = true;
                }
            }
            else
            {
                armourItem = static_cast<int16_t>(armourItem - amount);
                if (armourItem < 1) { armourItem = 0; }
            }
            damageCooldown = DamageCooldown();
            return true;
        }

        // FUN_0003e3e4: the same gates, but straight off health - armour does
        // not help. What a pit uses.
        bool ApplyHealthDamage(int amount, int cause)
        {
            if (damageCooldown > 0 || terrainShield > 0 || waitingAtExit) { return false; }
            lastDamageCause = cause;
            healthHits++;
            pendingSound = HURT_SOUND;   // 0x3e443
            health = static_cast<int16_t>(health - amount);
            if (health < 1)
            {
                health = 0;
                dead = true;
            }
            damageCooldown = DamageCooldown();
            return true;
        }

        // FUN_0003dff0's floor hazards, applied while the player is standing on
        // the floor (their height did not change this tick). Burning (7),
        // acid (8) and attribute 0xc hurt, unless the terrain speed item or
        // shield is running; a pit (10) kills regardless, clearing both
        // protections first.
        void ApplyFloorHazard(uint8_t attribute)
        {
            if (attribute == 10)
            {
                damageCooldown = 0;
                terrainShield = 0;
                ApplyHealthDamage(health, 5);
                return;
            }
            if (terrainSpeed != 0 || terrainShield != 0) { return; }
            switch (attribute)
            {
            case 7: ApplyDamage(5, 5); break;
            case 8: ApplyDamage(10, 5); break;
            case 0x0c: ApplyDamage(1, 5); break;
            default: break;
            }
        }

        // One gameplay tick of the cooldown.
        void Tick()
        {
            if (damageCooldown > 0) { damageCooldown--; }
        }
    };
}
