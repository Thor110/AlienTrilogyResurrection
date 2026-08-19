#pragma once

#include <cstdint>

namespace ALTEngine::Screens
{
    // Enemy behaviour, from FUN_00033cbc - the state machine that picks what an
    // entity does each time it needs a new action.
    //
    // ITS SHAPE. The function sets +0x68 = 0x3c and the state byte +0x6f = 5 on
    // entry, then branches first on +0x73 (below 2, or 2 and above) and inside
    // that on +0x74, the SUBTYPE. So behaviour is chosen by subtype, and +0x73
    // splits each subtype into two modes - the near/engaged case and the far one.
    //
    // What the arms do is start an animation: FUN_0002f4b0(entity, n) for most,
    // FUN_0002f1b0(entity, n) for subtypes 8 and 9, with FUN_00032f78 first for
    // the ones that turn to face a heading (+0x40).
    //
    // BY SUBTYPE, in the +0x73 < 2 branch:
    //     2   turn to +0x40, animation 10; if the frame timer is 0, set it 0x80.
    //         Also the special case: within range 0x601, if the global flag
    //         DAT_000b0ab4 bit 8 is clear and FUN_000303ec succeeds, it takes
    //         over the entity's own handler (installs LAB_00030918, sets 0x40
    //         and raises that flag). One at a time, globally - that is a grab or
    //         pounce that locks out the others while it plays.
    //     3   turn to +0x40, animation 0xc
    //     4   animation 0xc
    //     5   animation 10
    //     6   animation 0xb
    //     7   RANDOM of four: animation 0xb, animation 0xe, or animation 0xe
    //         plus raising bit 0x20 of +0xb6
    //     8   FUN_0002f1b0 with 9
    //     9   FUN_0002f1b0 with 8
    //     0xb, 0xc  animation 9
    //     0xd  animation 0x10
    //     0xe  animation 0xb
    //
    // In the +0x73 >= 2 branch the same subtypes mostly repeat, but 7 can also
    // write state 2 back into +0x6f on a coin flip - so it re-enters the machine
    // as a different subtype's behaviour rather than animating.
    //
    // The randomness is real, not a tie-break: subtype 7 picks from four options
    // every time it acts, which is what makes those creatures read as erratic.
    namespace EnemyBehaviour
    {
        // Entity fields this reads.
        inline constexpr int FIELD_MODE = 0x73;      // < 2 or >= 2 selects the branch
        inline constexpr int FIELD_SUBTYPE = 0x74;
        inline constexpr int FIELD_HEADING = 0x40;
        inline constexpr int FIELD_FRAME_TIMER = 0x32;
        inline constexpr int FIELD_STATE = 0x6f;
        inline constexpr int FIELD_FLAGS_B6 = 0xb6;

        inline constexpr int MODE_SPLIT = 2;
        inline constexpr int ACTING_STATE = 5;       // written on entry
        inline constexpr int ACT_TIMER = 0x3c;       // +0x68
        inline constexpr int DEFAULT_FRAME_TIMER = 0x80;

        // The pounce. Range is a squared/linear compare against +0x14; the flag
        // is bit 1 of DAT_000b0ab4's second byte and gates it globally so only
        // one creature can be doing it at a time.
        inline constexpr int POUNCE_RANGE = 0x601;
        inline constexpr int POUNCE_HANDLER_TIMER = 0x40;

        // Animation indices per subtype, for the near branch.
        inline int AnimationForSubtype(int subtype)
        {
            switch (subtype)
            {
            case 2:  return 10;
            case 3:  return 0xc;
            case 4:  return 0xc;
            case 5:  return 10;
            case 6:  return 0xb;
            case 0xb:
            case 0xc: return 9;
            case 0xd: return 0x10;
            case 0xe: return 0xb;
            default: return -1;   // 7 is random, 8 and 9 use the other starter
            }
        }

        // Subtypes 8 and 9 go through FUN_0002f1b0 rather than FUN_0002f4b0, and
        // with each other's number - 8 starts 9 and 9 starts 8.
        inline bool UsesAlternateStarter(int subtype) { return subtype == 8 || subtype == 9; }
        inline int AlternateAnimation(int subtype) { return (subtype == 8) ? 9 : 8; }

        // Subtype 7 picks one of these at random each time it acts.
        inline constexpr int RANDOM_SUBTYPE = 7;
        inline constexpr int RANDOM_CHOICES[4] = { 0xb, 0xe, 0xe, 0xc };

        // ---------------------------------------------------------------
        // PERCEPTION - what sets the mode, from FUN_00032154
        // ---------------------------------------------------------------
        //
        // Called once per tick from the entity update, before anything else. It
        // measures the player's separation on each axis into the entity's own
        // fields and then bands the distance:
        //
        //     entity[+0x10] = |playerX - entityX|      (field 4)
        //     entity[+0x14] = |playerY - entityY|      (field 5)
        //     entity[+0x18] = |playerZ - entityZ|      (field 6)
        //     distance      = FUN_000320f8(player, entity)  -> entity[+0x60]
        //
        //     distance < 0x400                 mode 0   touching
        //     distance < entity[+0x48] >> 16   mode 1   near, the entity's own
        //                                               first threshold
        //     distance < entity[+0x46] >> 16   mode 2   middle, second threshold
        //     distance <= 0x3fff               mode 3   far
        //     otherwise                        mode 4   very far
        //
        // SO THE NEAR/FAR SPLIT IS A DISTANCE BAND, and two of its thresholds are
        // PER CREATURE - read from the entity's own fields at +0x46 and +0x48
        // rather than being global. That is why the same subtype can behave
        // differently between creatures: an alien with a longer first threshold
        // switches to its close-range behaviour further out.
        //
        // FUN_00033cbc's two branches are "mode below 2" and "mode 2 or above",
        // so modes 0 and 1 are the engaged half and 2, 3 and 4 the disengaged one.
        inline constexpr int MODE_TOUCHING_RANGE = 0x400;
        inline constexpr int MODE_FAR_LIMIT = 0x3fff;
        inline constexpr int FIELD_THRESHOLD_NEAR = 0x48;   // per creature
        inline constexpr int FIELD_THRESHOLD_MIDDLE = 0x46; // per creature
        inline constexpr int FIELD_DISTANCE = 0x60;

        inline int ModeForDistance(int distance, int nearThreshold, int middleThreshold)
        {
            if (distance < MODE_TOUCHING_RANGE) { return 0; }
            if (distance < nearThreshold) { return 1; }
            if (distance < middleThreshold) { return 2; }
            return (distance > MODE_FAR_LIMIT) ? 4 : 3;
        }

        // ---------------------------------------------------------------
        // FUN_000358a4 IS THE VENT UPDATE, NOT THE GENERAL ENEMY TICK
        // ---------------------------------------------------------------
        //
        // CORRECTION to what was recorded here first. Reading it to the end shows
        // what it really is:
        //
        //   - Its first parameter is a TASK, not an entity: piVar1 = param_1[+8]
        //     is where the entity actually lives, so every offset below is
        //     through that indirection.
        //   - It opens by testing the monster type against 0x12 and 0x10 - the
        //     VERTICAL and HORIZONTAL STEAM VENTS - and sets a flag when it
        //     matches, which then picks between two sounds (0x1c and 0x68) and
        //     negates a direction.
        //   - Its "state 8" arm, on a timer measured against +0x4a and +0x5e,
        //     calls FUN_0002b0e4 - a PARTICLE SPAWNER - with the entity and a
        //     direction. That is the jet itself, emitted on a cycle.
        //
        // So this is the steam and flame vent task. Vents are entities occupying
        // monster types 0x10-0x13, so they share the entity fields and the state
        // byte, which is exactly why it looked like the general tick: perception,
        // a distance cull and a state switch are all there, because a vent needs
        // them too.
        //
        // The structure recorded below is therefore right about VENTS and unproven
        // as a description of creatures. Both share the entity layout, so the
        // field meanings hold; what is not established is that a creature runs
        // this same function.
        //
        // AND IT GIVES THE STEAM AND FLAME NOTICES A HOME: "Steam valve closed"
        // and "Flame jet shut down" belong to whatever stops this task.
        //
        // ---------------------------------------------------------------
        // THE VENT TICK - FUN_000358a4
        // ---------------------------------------------------------------
        //
        // The order is: bump the entity's own tick counter, wrapping at its
        // +0x46 threshold; check the monster record's trigger gate (byte 7 against
        // byte 8) and if it has not fired yet, count the creature into
        // DAT_000b0ae6[type] and play a sound; otherwise run perception and then
        // switch on the STATE byte +0x6f:
        //
        //     state 0   alive and acting. If its health field +0x58 has dropped
        //               to zero or below, go to state 8 - dead.
        //     state 6   the hit reaction has finished: clear the timers, return to
        //               state 0 and set +0x52 to 1.
        //     state 7   dying: spawn the death effect at its position
        //               (FUN_0002aaf0) and go to state 8.
        //     state 8   dead. Held until a timer measured against +0x4a and +0x5e
        //               expires.
        //     default   anything else drops to state 6, the hit reaction.
        //
        // AND IT CULLS AT 0x1e00: if the perceived distance exceeds that, the tick
        // returns immediately and the entity does nothing at all. So distant
        // enemies are not simulated, which matters for the port - an enemy across
        // the level should not be pathing toward the player.
        inline constexpr int SIMULATION_CUTOFF = 0x1e00;
        inline constexpr int FIELD_HEALTH = 0x58;
        inline constexpr int STATE_ACTING = 0;
        inline constexpr int STATE_HIT_RECOVER = 6;
        inline constexpr int STATE_DYING = 7;
        inline constexpr int STATE_DEAD = 8;

        // ---------------------------------------------------------------
        // ATTACKS - FUN_00033ff8, and this is the piece that makes enemies
        // actually dangerous
        // ---------------------------------------------------------------
        //
        // Sixteen of its call sites reach FUN_00033cbc, which is why that
        // function appeared to have no callers - Ghidra had not decompiled the
        // region containing them.
        //
        // Its shape is the same as the action chooser's: branch on the mode at
        // +0x73, switch on the subtype at +0x74. Each arm runs two checks and
        // then deals damage:
        //
        //     FUN_00033a1c(entity)                        can it attack at all
        //     FUN_0003231c(entity, entity[+0x52] >> 16)   is the player in front
        //     then FUN_0003e5a8(damage, 4)                a melee hit, or
        //          FUN_0003e5d4(damage, angle)            a directed hit, with
        //                                                 the angle taken from
        //                                                 the entity's +0x42
        //                                                 heading plus 0x800
        //
        // FUN_0003e5a8 is the same player-damage entry the blast uses
        // (FUN_0003e5a8(0x32, 5) for an explosion), so these numbers are directly
        // comparable: a barrel does 50, a face hugger does 5.
        //
        // THE TABLE, damage by subtype and distance band:
        //
        //     subtype   mode 0/1 (close)   mode 2 (middle)
        //       2, 3     5   melee          1   melee
        //       4, 5    10   directed       5   directed
        //       6       15   directed      10   directed
        //       7       25   directed      15   directed
        //
        // Two things fall out of that. Damage SCALES WITH RANGE - the same
        // creature hits for a fifth as much at middle distance as up close, so a
        // hugger brushing past is a scratch and one on your face is not. And
        // subtypes 2 and 3 are the only melee attackers; everything from 4 up
        // deals its damage with a direction attached, which is what drives the
        // view kick.
        //
        // Nothing attacks from mode 3 or 4 - the far bands have no arms at all.
        inline constexpr int ATTACK_ANGLE_OFFSET = 0x800;

        // THE TWO GATES, and neither is movement - I had guessed one of them
        // might be the approach step and it is not.
        //
        // FUN_00033a1c is a FACING CONE with a visibility test in front of it:
        //
        //     if ((entity[+0x78].cellFlags & DAT_000b0cf0) >> 16 == 0) -> false
        //     delta = |entity[+0x42] - entity[+0x54]|
        //     return delta <= entity[+0x40]
        //
        // +0x42 is the creature's heading and +0x54 the bearing to the player, so
        // the difference is how far off it is pointing, and +0x40 is its own cone
        // half-width - PER CREATURE, like the distance thresholds. The first test
        // reads a mask out of the entity's CELL through +0x78, which is a
        // visibility check: a creature in a cell the player cannot see does not
        // attack.
        //
        // FUN_0003231c is the line of fire. It builds three probe directions from
        // the passed angle - plus 0x800, plus 0x400, and plus 0xc00 - and traces
        // them, so it is checking a spread rather than a single ray, and it
        // refuses outright beyond 0x2000.
        //
        // So an attack needs: the cell visible, the player inside the cone, and
        // the line of fire clear. Movement is somewhere else entirely.
        inline constexpr int FIELD_BEARING_TO_PLAYER = 0x54;
        inline constexpr int FIELD_CONE_HALF_WIDTH = 0x40;   // per creature
        inline constexpr int FIELD_CELL = 0x78;
        inline constexpr int LINE_OF_FIRE_LIMIT = 0x2000;

        // A creature can attack if it is pointing close enough at the player.
        // `coneHalfWidth` is the entity's own +0x40.
        inline bool WithinAttackCone(int heading, int bearingToPlayer, int coneHalfWidth)
        {
            int delta = heading - bearingToPlayer;
            if (delta < 0) { delta = -delta; }
            return delta <= coneHalfWidth;
        }

        struct AttackProfile
        {
            int closeDamage;    // modes 0 and 1
            int middleDamage;   // mode 2
            bool melee;         // false = directed, carrying an angle
        };

        inline AttackProfile AttackForSubtype(int subtype)
        {
            switch (subtype)
            {
            case 2:
            case 3:  return { 5, 1, true };
            case 4:
            case 5:  return { 10, 5, false };
            case 6:  return { 15, 10, false };
            case 7:  return { 25, 15, false };
            default: return { 0, 0, false };   // no attack arm
            }
        }

        // Returns the damage for a subtype at a given mode, or 0 if it cannot
        // attack from there.
        inline int AttackDamage(int subtype, int mode)
        {
            const AttackProfile profile = AttackForSubtype(subtype);
            if (mode <= 1) { return profile.closeDamage; }
            if (mode == 2) { return profile.middleDamage; }
            return 0;
        }

        // ---------------------------------------------------------------
        // MOVEMENT - FUN_00031f3c, found at last
        // ---------------------------------------------------------------
        //
        // It is the one function in the enemy region that writes an entity's
        // position, and it does everything a mover should:
        //
        //   1. Works in a SHIFTED SPACE. Every position is offset by
        //      DAT_000b0c5c and DAT_000b0c5e on the way in and unshifted on the
        //      way out, and it bails immediately if either goes negative. That is
        //      the same world-origin offset the port already applies.
        //
        //   2. Samples the floor AND the ceiling under the new position:
        //          entity[+0x5a] = FUN_00027e28(x, z)   floor
        //          entity[+0x5c] = FUN_00027fb0(x, z)   ceiling
        //      then stands the entity 0x20 above the floor - or 0x20 BELOW THE
        //      CEILING when flag 8 of +0x6c is set. That is how a ceiling
        //      creature is held up there: one flag, same mover.
        //
        //   3. Applies vertical velocity at +0x32 when non-zero, decelerating by
        //      0xc a tick and clamping to the floor and ceiling - a fall or a
        //      drop, and it self-terminates by zeroing the velocity when it
        //      lands.
        //
        //   4. Steps the two horizontal axes through separate helpers -
        //      FUN_000315f0 for one and FUN_00031afc for the other, each gated on
        //      its own velocity field (+0x30 and +0x34). Those are where the
        //      collision response lives.
        //
        //   5. Recomputes the occupied CELL from the new position
        //          cell = grid + width * (z >> 9) * 0x10 + (x >> 9) * 0x10
        //      and when it changes, restamps the cell's occupancy bytes and calls
        //      FUN_0004129c. The >> 9 is the same 512-unit cell size the port
        //      uses.
        //
        // THE CEILING FLAG IS THE INTERESTING PART. Flag 8 at +0x6c switches the
        // entity from standing on the floor to hanging from the ceiling, and
        // clearing it would drop the creature through the normal fall path in
        // step 3. So DOGCEIL and WARCEIL are not a separate movement system at
        // all - they are ordinary creatures with one bit set, and "they never drop
        // down" (Edward, 2026) means nothing ever clears that bit. That is a much
        // narrower thing to look for than a missing behaviour.
        inline constexpr int FIELD_FLOOR_SAMPLE = 0x5a;
        inline constexpr int FIELD_CEILING_SAMPLE = 0x5c;
        inline constexpr int FIELD_VERTICAL_VELOCITY = 0x32;
        inline constexpr int FIELD_VELOCITY_A = 0x30;
        inline constexpr int FIELD_VELOCITY_B = 0x34;
        inline constexpr int FIELD_MOVE_FLAGS = 0x6c;

        inline constexpr int STAND_CLEARANCE = 0x20;   // above floor, or below ceiling
        inline constexpr int FALL_DECELERATION = 0xc;
        // ENTITY STATUS FLAGS, byte at +0x6c. NOT the animator's flags.
        //
        // These are easy to confuse and I confused them: the animation VM has its
        // own flag byte with FLAG_USER8 = 0x08, set by opcode 9 (OP_SET_FLAG8).
        // That byte lives at the ANIMATOR base, which for an entity is +0x80, so
        // it is at +0xb6 - a completely different byte from this one. The bit
        // numbers happen to match, which is the whole of the resemblance.
        //
        // What actually touches +0x6c:
        //     | 0x01   FUN_00033cbc, the action chooser
        //     | 0x02   FUN_0002f288 (crate release) and FUN_00035530
        //     | 0x20   FUN_000313b0, the damage function
        //     & 0xf7   FUN_0002f1b0 and FUN_00030b04 - these CLEAR bit 8
        // and FUN_00027b90 initialises the whole word from the template as
        //     *(short*)(+0x6c) = *(short*)(+0x68) << 8
        // which leaves the low byte - this one - at zero.
        //
        // THE INTERESTING PART, and it revises what was said here before. Bit 8
        // IS cleared, in two places:
        //
        //   FUN_00030b04  the death path - so a dying ceiling creature falls.
        //   FUN_0002f1b0  the ALTERNATE animation starter, and the only subtypes
        //                 that use it are 8 and 9 - the CEILING WARRIOR and the
        //                 CEILING DOG (FUN_00033cbc's arms for those two call it
        //                 rather than FUN_0002f4b0).
        //
        // So the drop mechanism exists and belongs to exactly the creatures that
        // need it: when a ceiling creature picks a new action, starting that
        // action clears the flag and the mover drops it. "They never drop down"
        // therefore points at the AI never reaching that arm for them, not at a
        // missing behaviour - and those same subtypes are the ones drawn by the
        // OTHER draw path in FUN_000300a4, which is still unread.
        //
        // AND THE SET IS FOUND - IT HAPPENS AT SPAWN.
        //
        // In FUN_0002e638, two of the spawn arms do:
        //     0002eb43   OR  CL,0x8
        //     0002eb51   MOV byte ptr [EBX + 0x6c],CL
        //     0002ebab   OR  CH,0x8
        //     0002ebb9   MOV byte ptr [EBX + 0x6c],CH
        // and the animation bases those two arms then install are 0x000ac4f0 and
        // 0x000ac460 - two of the fifteen creature tables.
        //
        // The DECOMPILER HID THIS. It rendered both as plain assignments -
        // `*(byte*)(entity + 0x6c) = cVar;` - with the OR folded into the value,
        // so searching the decompiled text for `| 8` found nothing and I twice
        // concluded the set did not exist. It is plainly there in the
        // disassembly. Worth remembering: when a search of the decompiled set
        // comes back empty, check the instructions before believing it.
        //
        // So the full life of the flag is:
        //     set     at spawn, for the two ceiling creature arms
        //     cleared by FUN_0002f1b0 when one of those creatures starts an
        //             action, which drops it, and by FUN_00030b04 on death
        //
        // The mechanism is therefore complete and self-consistent: they spawn on
        // the ceiling and fall when they act. Whatever stops them acting in the
        // original is a separate question, and now a much better defined one.
        inline constexpr int CEILING_FLAG = 8;         // bit 8 of the byte at +0x6c
        inline constexpr int ENTITY_FLAG_ACTING = 0x01;
        inline constexpr int ENTITY_FLAG_SPRUNG = 0x02;
        inline constexpr int ENTITY_FLAG_DAMAGED = 0x20;

        // Where an entity's body sits for a given floor and ceiling.
        inline int RestingHeight(int floorHeight, int ceilingHeight, bool onCeiling)
        {
            return onCeiling ? (ceilingHeight - STAND_CLEARANCE)
                             : (floorHeight + STAND_CLEARANCE);
        }

        // ---------------------------------------------------------------
        // THE HORIZONTAL STEP - FUN_000315f0 and FUN_00031afc
        // ---------------------------------------------------------------
        //
        // One per axis, each gated on its own velocity field. Both work the same
        // way, and it is a FOOTPRINT test rather than a point test:
        //
        //   - The candidate position is pushed 200 units further along the axis
        //     of travel, in the direction of the velocity's sign, so the probe
        //     leads the creature rather than centring on it.
        //   - THREE cells are then sampled across the other axis, at -200, 0 and
        //     +200. So a creature occupies a 400-unit span and needs all three
        //     clear, which is why they do not clip corners.
        //   - A cell blocks when its attribute is in the band 0x14..0x2c
        //     inclusive - the same wall band the port already uses - and
        //     separately when its occupancy byte at +0xc is non-zero, which is
        //     another entity standing there.
        //   - When occupancy blocks but the walls do not, it calls FUN_000315b4
        //     to resolve rather than simply stopping. That is a slide.
        //
        // The 200-unit half-width is worth having: it is the creature's collision
        // radius, and it is the same for every creature.
        inline constexpr int MOVE_PROBE_LEAD = 200;
        inline constexpr int MOVE_FOOTPRINT_HALF = 200;
        inline constexpr int BLOCKING_ATTRIBUTE_MIN = 0x14;
        inline constexpr int BLOCKING_ATTRIBUTE_MAX = 0x2c;

        inline bool AttributeBlocks(int attribute)
        {
            return attribute >= BLOCKING_ATTRIBUTE_MIN && attribute <= BLOCKING_ATTRIBUTE_MAX;
        }

        // ---------------------------------------------------------------
        // ANIMATION INDEX BY BEHAVIOUR AND SUBTYPE
        // ---------------------------------------------------------------
        //
        // This is the piece that was blocking everything: the animation tables
        // and the frame records were readable, but nothing said WHICH animation a
        // creature should be playing. The three behaviours that pick one all do
        // it with a switch on the subtype at +0x74.
        //
        // ACTING - FUN_00033cbc, the near branch (mode below 2):
        //     2 -> 10    3 -> 12    4 -> 12    5 -> 10    6 -> 11
        //     7 -> random of 11, 14, 14 and 12
        //     8 -> 9 and 9 -> 8, both through the other starter
        //     11, 12 -> 9    13 -> 16    14 -> 11
        //
        // TAKING A HIT - FUN_000310ac:
        //     1 -> 3, and 2 through 6 -> 8, 7 through 10 -> 8, default -> 9 or 4
        //   So nearly every creature shares animation 8 for its flinch.
        //
        // DYING - FUN_00030b04:
        //     1 -> 1     2 -> 9     3 -> 11    4 -> 11    5 -> 13
        //     6 -> 10    7 -> 15    8 -> 8     9 -> 10    10 -> 25
        //     default -> 10
        //
        // So a face hugger walks on 10, flinches on 8 and dies on 9. A warrior
        // drone walks on 11, flinches on 8 and dies on 10.
        //
        // TWO SUBTYPES REWRITE THEMSELVES AS THEY DIE. In FUN_00030b04, one arm
        // sets +0x74 = 6 before starting animation 8 and another sets +0x74 = 5
        // before animation 10 - so the creature becomes a different subtype on
        // death. That is presumably how a ceiling creature turns into its ground
        // form to fall, and it is worth knowing before treating the subtype as
        // fixed.
        inline int ActingAnimation(int subtype)
        {
            switch (subtype)
            {
            case 2:  return 10;
            case 3:  return 12;
            case 4:  return 12;
            case 5:  return 10;
            case 6:  return 11;
            case 11:
            case 12: return 9;
            case 13: return 16;
            case 14: return 11;
            default: return -1;   // 7 is random, 8 and 9 use the other starter
            }
        }

        inline int HitAnimation(int subtype)
        {
            if (subtype == 1) { return 3; }
            if (subtype >= 2 && subtype <= 10) { return 8; }
            return 9;
        }

        inline int DeathAnimation(int subtype)
        {
            switch (subtype)
            {
            case 1:  return 1;
            case 2:  return 9;
            case 3:  return 11;
            case 4:  return 11;
            case 5:  return 13;
            case 6:  return 10;
            case 7:  return 15;
            case 8:  return 8;
            case 9:  return 10;
            case 10: return 25;
            default: return 10;
            }
        }

        // THE DECISION TIMER. FUN_00033cbc writes +0x68 = 0x3c on entry and
        // nothing in the entity region decrements it - the only reference to that
        // field anywhere in 0x2e000..0x37000 is that one write. So the countdown
        // is consumed somewhere not yet read, and 0x3c is what it starts at.
        inline constexpr int DECISION_INTERVAL_TICKS = 0x3c;

        // ---------------------------------------------------------------
        // THE FACE HUGGER ON THE CAMERA - FINGERS.B16
        // ---------------------------------------------------------------
        //
        // FINGERS.B16 is the overlay of the creature crawling onto the player's
        // face (Edward, 2026). That resolves the loose end from the type table:
        // FINGERS has no monster type of its own because it is not a creature in
        // the world - it is what the pounce draws over the view.
        //
        // FUN_000303ec is the gate, called from the action chooser's subtype 2
        // arm within range POUNCE_RANGE. It claims TWO free sprite slots out of
        // the same cache the entity sprites use (DAT_000a4d90, the table with 12
        // slots, walking for entries whose byte at +7 is zero), and only proceeds
        // if it gets both. So a pounce cannot start when the sprite cache is
        // full - which is a second reason, besides the global exclusivity bit,
        // that only one can be happening at a time.
        //
        // It then reads a frame table from the creature's own animation table at
        // +0x8c and +0x88 - PAST the animation pairs - and sets _DAT_002479c0 to
        // 0xa0. Those two pointers past the pairs are the same fields I noticed
        // earlier when the animation table looked longer than its pair count;
        // they are the face-crawl frames.
        //
        // NOT IMPLEMENTED. What is here is the entry condition and where the
        // frames live, not the overlay itself.
        inline constexpr int POUNCE_SLOTS_NEEDED = 2;
        inline constexpr int ANIM_TABLE_FACE_FRAMES_A = 0x88;
        inline constexpr int ANIM_TABLE_FACE_FRAMES_B = 0x8c;

        // ---------------------------------------------------------------
        // FOLLOW UP: the ceiling creatures
        // ---------------------------------------------------------------
        //
        // DOGCEIL and WARCEIL are the ceiling variants - separate NME files and
        // separate animation tables from DOG and WAR, so the game treats them as
        // their own creatures rather than a mode of the ground ones.
        //
        // KNOWN ISSUE, observed in the original: they sit on the ceiling and
        // never drop or attack from up there, even though the artwork has a
        // frame for it (Edward, 2026). Reachable quickly through Level Select on
        // the later levels.
        //
        // Worth knowing before chasing it: this state machine is reached only
        // when an entity needs a NEW action, and every arm here either starts an
        // animation or hands control to another handler. A creature that never
        // drops is therefore either never being asked to act, or is being asked
        // and taking an arm that keeps it where it is. The +0x73 mode split is
        // the first thing to look at, since that is what separates "engaged" from
        // "idle" per subtype - a ceiling creature stuck in the far branch would
        // behave exactly like this.
        //
        // WHERE THIS STANDS - see the note on the +0x6c flags above.
        //
        // The drop mechanism is found: FUN_0002f1b0 clears the ceiling bit, and
        // it is the animation starter used by subtypes 8 and 9 specifically. So
        // acting drops them.
        //
        // The set is found too - FUN_0002e638 raises it at spawn for the two
        // ceiling creature arms. So the whole cycle is accounted for: spawn on
        // the ceiling, drop on the first action, drop on death.
        //
        // That makes "they never drop or attack up there" a question about the
        // AI never choosing an action for them, which points at the same two
        // subtypes taking the unread draw path in FUN_000300a4 and at whatever
        // gates their acting.
        //
        // Still NOT worked around, and still not implemented - but the mechanism
        // is now understood end to end rather than guessed at.
        inline constexpr bool CEILING_DROP_IMPLEMENTED = false;
    }
}
