#pragma once

#include "PlayerHudState.h"

#include <array>

namespace ALTEngine::Screens
{
    // The weapon state machine, transcribed from the original.
    //
    //   FUN_000401d0  selects a weapon; sets the per-weapon state table
    //                 (fix_off32_000ace58/48/28/88/70) and its graphic
    //   FUN_000400fc  (re)starts the animation for the CURRENT state
    //   FUN_0003efcc  reads the fire keys, and separately runs the
    //                 out-of-ammo / reload checks when the weapon is idle
    //   FUN_0003d5b0  primary fire
    //   FUN_0003e93c  per-tick: ticks the animator, plays the animation's
    //                 sound events, and returns the weapon to idle when the
    //                 sequence ends
    //
    // The key structure to understand: DAT_000b0aac packs TWO values. Its low
    // half is the weapon STATE and its high half is the weapon INDEX. The state
    // is what indexes the per-weapon 8-byte table, so each weapon has one
    // animation sequence per state, and changing state is what plays an
    // animation.
    namespace WeaponSystem
    {
        // Weapon state, the low half of DAT_000b0aac.
        //
        // 0, 1, 2 and 4 are certain - they appear as literal stores next to
        // behaviour that identifies them. 3 is the flamethrower's grenade, also
        // certain. 5 only ever appears in tests alongside 4 as a state the
        // smartgun must not fire from, so what it IS is not traced.
        enum State
        {
            STATE_IDLE = 0,
            STATE_FIRING = 1,
            STATE_RELOAD = 2,
            STATE_GRENADE = 3,   // flamethrower secondary only
            STATE_EMPTY = 4,     // out of ammo; will not fire from here
            STATE_UNKNOWN_5 = 5, // untraced - see above
            STATE_COUNT = 6,
        };

        // How the primary fire key is read. TRACED, and it differs per weapon:
        // FUN_0003efcc tests the pistol and shotgun against DAT_000b0cc4 (the
        // newly-pressed bits) and the flamethrower and pulse rifle against
        // DAT_000b0cc8 (the currently-held bits). That is the difference between
        // one shot per press and holding the trigger down.
        enum FireMode
        {
            FIRE_ON_PRESS,   // pistol, shotgun
            FIRE_WHILE_HELD, // flamethrower, pulse rifle, smartgun
        };

        struct WeaponDef
        {
            int magazineSize;  // rounds restored by a reload; 0 = no magazine
            int noise;         // DAT_000b0adc, set on every shot
            FireMode primary;
        };

        // Per-weapon constants, all read directly from the decompilation.
        //
        //   magazineSize: FUN_0003efcc's reload arms - 0xf for the pistol, 100
        //   for the flamethrower's tank and the pulse rifle's magazine. The
        //   shotgun and smartgun have no unit counter and so never reload.
        //   These agree with PlayerHudState::ROUNDS_PER_UNIT, which was derived
        //   independently from the HUD's own display arithmetic.
        //
        //   noise: DAT_000b0adc, set by FUN_0003d5b0 per weapon.
        inline constexpr std::array<WeaponDef, PlayerHudState::WEAPON_COUNT> WEAPONS{ {
            { 0xf,  0x80,  FIRE_ON_PRESS },   // 0 9mm pistol
            { 0,    0x200, FIRE_ON_PRESS },   // 1 shotgun
            { 100,  0x140, FIRE_WHILE_HELD }, // 2 pulse rifle
            { 100,  0x200, FIRE_WHILE_HELD }, // 3 flamethrower
            { 0,    0x80,  FIRE_WHILE_HELD }, // 4 smartgun
            //
            // ONLY THE LABELS CHANGED HERE. The values were already read out of
            // FUN_0003d5b0 and FUN_0003efcc in the switch's own case order, so
            // they were right all along - it was the names against them that had
            // the pulse rifle and the flamethrower the wrong way round.
        } };

        // The flamethrower's grenade, fired from its secondary. Its own noise
        // value, the loudest in the game (FUN_0003efcc case 2, FUN_0003d794).
        inline constexpr int GRENADE_NOISE = 0x280;

        // Weapon sprite placement on the 320x240 HUD surface, from FUN_0003e93c:
        //     x = 0xa0                        + STATE_OFFSET_X[state]
        //     y = 0xf0 - (bobTerm) + b57      + STATE_OFFSET_Y[state]
        // 0xa0 is 160, the horizontal centre, and 0xf0 is 240, the bottom edge -
        // so the anchor is bottom-centre, which is what was already guessed here.
        inline constexpr int WEAPON_ANCHOR_X = 0xa0;
        inline constexpr int WEAPON_ANCHOR_Y = 0xf0;

        // Per-weapon nudge, from the 4-byte-stride table at DAT_000acea2 (x) /
        // DAT_000acea4 (y). RESOLVED from the data listing.
        //
        // I had this indexed by STATE. It is not - FUN_0003e93c indexes it with
        // `DAT_000b0aac >> 0x10`, which is the HIGH half of that packed word,
        // i.e. the weapon. The low half is the state. Six entries, five weapons
        // and a spare, which is the giveaway I should have caught.
        //
        // Every Y is zero, so the correction is purely horizontal - each weapon
        // sprite has its own idea of where its centre is.
        inline constexpr std::array<int, PlayerHudState::WEAPON_COUNT + 1> WEAPON_OFFSET_X{
            +4,  // 0 pistol
            +2,  // 1 shotgun
             0,  // 2 flamethrower
            -6,  // 3 pulse rifle
            +10, // 4 smartgun
             0,  // spare slot
        };
        inline constexpr std::array<int, PlayerHudState::WEAPON_COUNT + 1> WEAPON_OFFSET_Y{
            0, 0, 0, 0, 0, 0
        };

        // DAT_000b0b57, added to the weapon's Y. Untraced; zero until it is.
        inline constexpr int WEAPON_Y_BIAS = 0; // GUESS

        // The weapon's vertical bob, from FUN_0003e93c's `_DAT_000b0b4e >> 0x14`.
        //
        // That packed value is CONCAT22(dip >> 6, dip >> 9) from FUN_0003d00c,
        // so an arithmetic shift of 20 leaves (dip >> 10). `dip` is the camera's
        // own -|sin(phase)| term, running 0 to -4096, which puts this at 0 to -4
        // and moves the weapon DOWN by up to 4 pixels as the head dips. The gun
        // rides the same bob the view does, from the same phase.
        inline constexpr int WeaponBobOffset(int cameraDip)
        {
            return -(cameraDip >> 10);
        }

        // Runtime state - the two halves of DAT_000b0aac plus the noise timer.
        struct Runtime
        {
            int weapon = 0;              // DAT_000b0aac high half
            int state = STATE_IDLE;      // DAT_000b0aac low half
            int noise = 0;               // DAT_000b0adc
            bool stateChanged = false;   // set on any transition, so the caller
                                         // knows to restart the animation
                                         // (the original's FUN_000400fc call)

            void SetState(int next)
            {
                state = next;
                stateChanged = true;
            }
        };

        inline const WeaponDef& Def(int weapon)
        {
            if (weapon < 0 || weapon >= PlayerHudState::WEAPON_COUNT) { return WEAPONS[0]; }
            return WEAPONS[static_cast<size_t>(weapon)];
        }

        // FUN_0003d5b0. Fires if there is a round chambered.
        //
        // The ammo decrement is NOT in FUN_0003d5b0 - it lives in each weapon's
        // projectile spawn (FUN_0002bb74 does `DAT_000b0abe--` for the pistol,
        // and the smartgun's FUN_0002c0a0 subtracts a variable amount). Since
        // projectiles do not exist yet, the decrement is done here instead, by
        // one. MARKED: the smartgun's real rate is a variable this cannot see.
        //
        // Returns true if a shot went off.
        inline bool TryPrimaryFire(Runtime& runtime, PlayerHudState& hud)
        {
            if (runtime.weapon < 0 || runtime.weapon >= PlayerHudState::WEAPON_COUNT) { return false; }
            const size_t w = static_cast<size_t>(runtime.weapon);

            // Every weapon except the smartgun refuses to fire unless idle, so
            // the firing animation's own length sets the rate of fire. The
            // smartgun keeps spawning while held and only restarts the
            // animation from idle (FUN_0003d5b0 case 4).
            const bool smartgun = (runtime.weapon == 4);
            if (!smartgun && runtime.state != STATE_IDLE) { return false; }
            if (smartgun && (runtime.state == STATE_EMPTY || runtime.state == STATE_UNKNOWN_5)) { return false; }

            if (hud.ammoRemainder[w] == 0) { return false; }

            runtime.noise = Def(runtime.weapon).noise;
            if (runtime.state == STATE_IDLE) { runtime.SetState(STATE_FIRING); }

            hud.ammoRemainder[w] = static_cast<int16_t>(hud.ammoRemainder[w] - 1);
            return true;
        }

        // The second switch in FUN_0003efcc: run only while idle, this is what
        // reloads a spent magazine or drops the weapon into its empty state.
        //
        // `canSwitchAway` stands in for FUN_00038c78, which the original calls
        // to hand off to another weapon that still has ammo. Its fallback is
        // distinctive and worth keeping: when it CANNOT switch, the code sets
        // the PISTOL's remainder to 1 - not the current weapon's. So running
        // completely dry always leaves one pistol round, which is why the player
        // is never left with nothing at all.
        inline void UpdateIdle(Runtime& runtime, PlayerHudState& hud, bool canSwitchAway)
        {
            if (runtime.state != STATE_IDLE) { return; }
            if (runtime.weapon < 0 || runtime.weapon >= PlayerHudState::WEAPON_COUNT) { return; }

            const size_t w = static_cast<size_t>(runtime.weapon);
            if (hud.ammoRemainder[w] != 0) { return; }

            const WeaponDef& def = Def(runtime.weapon);

            // A magazine weapon with spare units reloads.
            if (def.magazineSize > 0 && hud.ammoUnits[w] > 0)
            {
                hud.ammoUnits[w] = static_cast<int16_t>(hud.ammoUnits[w] - 1);
                hud.ammoRemainder[w] = static_cast<int16_t>(def.magazineSize);
                runtime.SetState(STATE_RELOAD);
                return;
            }

            // Nothing left. Hand off if anything else can fire, otherwise fall
            // back to the pistol as described above.
            if (!canSwitchAway)
            {
                if (hud.ammoRemainder[0] == 0) { hud.ammoRemainder[0] = 1; }
            }
            runtime.SetState(STATE_EMPTY);
        }

        // FUN_0003e698 decrements DAT_000b0adc by one per tick.
        //
        // WHAT IT IS FOR IS INFERENCE, NOT TRACED. It is set per shot, decays,
        // and is ordered exactly the way the weapons are ordered by loudness -
        // pistol 0x80, flamethrower 0x140, shotgun and pulse rifle 0x200,
        // grenade 0x280. A decaying per-shot magnitude with that ordering reads
        // as a noise level for enemies to hear, but nothing that CONSUMES it has
        // been traced yet, so treat the name as provisional.
        inline void TickNoise(Runtime& runtime)
        {
            if (runtime.noise > 0) { runtime.noise--; }
        }

        // Whether entering a state starts an animation.
        //
        // NOT ALL OF THEM DO. FUN_0003efcc's out-of-ammo arms set state 4 and
        // call FUN_000524b0, NOT FUN_000400fc - so the empty state deliberately
        // leaves the previous frame on screen rather than playing anything. The
        // per-weapon table backs this up: its entries are contiguous in memory
        // between the five weapons (0xace28, 0xace48, 0xace58, 0xace70,
        // 0xace88), which leaves the pistol three states, the shotgun two and
        // the flamethrower four - too few for a state-4 entry to exist at all.
        inline constexpr bool StateHasAnimation(int state)
        {
            return state == STATE_IDLE || state == STATE_FIRING
                || state == STATE_RELOAD || state == STATE_GRENADE;
        }

        // FUN_0003e93c: the animation ending is what returns the weapon to idle.
        // States 4 and 5 are excluded - the original leaves them latched.
        inline void OnAnimationEnded(Runtime& runtime)
        {
            if (runtime.state == STATE_EMPTY || runtime.state == STATE_UNKNOWN_5) { return; }
            runtime.SetState(STATE_IDLE);
        }
    }
}
