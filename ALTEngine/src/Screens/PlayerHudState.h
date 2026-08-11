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

        // 0x000b0ab8 and 0x000b0aba. Armour absorbs damage FIRST; health is only
        // touched once armour is gone.
        int16_t health = 100;
        int16_t armour = 0;

        // 0x000b0aae. Which weapon's ammo the HUD shows.
        int16_t currentWeapon = 0;

        // Ammo is NOT one counter per weapon. Two weapons store a unit count
        // plus a remainder and the HUD adds them together; the other three are a
        // single counter. Kept in that shape rather than flattened to one number
        // per weapon, because pickups add to the unit counter and firing
        // decrements the remainder - flattening would lose that distinction and
        // make reload behaviour impossible to reproduce.
        //
        //   weapon 0: units x 15  + remainder   (0x000b0ac0, 0x000b0abe)
        //   weapon 1: remainder only            (0x000b0ac2)
        //   weapon 2: units x 100 + remainder   (0x000b0ac6, 0x000b0ac4)
        //   weapon 3: units x 100 + remainder   (0x000b0ace, 0x000b0acc)
        //   weapon 4: remainder only            (0x000b0aca)
        std::array<int16_t, WEAPON_COUNT> ammoUnits{ 0, 0, 0, 0, 0 };
        std::array<int16_t, WEAPON_COUNT> ammoRemainder{ 0, 0, 0, 0, 0 };

        // Rounds per unit for each weapon; 0 means the weapon has no unit
        // counter and its total is the remainder alone.
        static constexpr std::array<int, WEAPON_COUNT> ROUNDS_PER_UNIT{ 15, 0, 100, 100, 0 };

        // 0x000b0b30. Ticks of invulnerability after a hit - while non-zero the
        // original skips its whole damage path.
        int16_t damageCooldown = 0;

        bool dead = false; // DAT_000b0cc0 bit 0x20

        // What the HUD prints for a weapon, exactly as FUN_0003aac8's switch
        // computes it.
        int AmmoTotal(int weapon) const
        {
            if (weapon < 0 || weapon >= WEAPON_COUNT) { return 0; }
            size_t i = static_cast<size_t>(weapon);
            return ROUNDS_PER_UNIT[i] * ammoUnits[i] + ammoRemainder[i];
        }

        int CurrentAmmoTotal() const { return AmmoTotal(currentWeapon); }

        // Applies damage the original's way: cooldown gates it entirely, armour
        // absorbs before health, both clamp at zero, and reaching zero health
        // sets `dead`.
        //
        // Returns true if the hit landed (i.e. was not swallowed by the
        // cooldown), so a caller can decide whether to play a reaction.
        bool ApplyDamage(int amount, int cooldownTicks = 30)
        {
            if (damageCooldown > 0) { return false; }
            if (amount <= 0) { return false; }

            if (armour > 0)
            {
                armour = static_cast<int16_t>(armour - amount);
                if (armour < 0) { armour = 0; }
            }
            else
            {
                health = static_cast<int16_t>(health - amount);
                if (health < 1)
                {
                    health = 0;
                    dead = true;
                }
            }

            damageCooldown = static_cast<int16_t>(cooldownTicks);
            return true;
        }

        // One gameplay tick of the cooldown.
        void Tick()
        {
            if (damageCooldown > 0) { damageCooldown--; }
        }
    };
}
