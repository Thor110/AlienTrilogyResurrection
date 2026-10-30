#pragma once

#include "../Formats/OriginalTables.h"

#include <cmath>
#include <cstdint>

#include "WeaponSystem.h"

namespace ALTEngine::Screens
{
    // The player's camera - turning, walking, automatic pitch and head bob,
    // transcribed from the original rather than approximated.
    //
    // THE CHAIN IN THE ORIGINAL:
    //   FUN_0003efcc  input -> intent (turn rate, speed, move angle, pitch)
    //   FUN_0003dff0  apply movement, collision, floor follow, terrain damage
    //   FUN_0003d00c  build the view pose from the player pose - INCLUDING BOB
    //
    // This header covers FUN_0003efcc and FUN_0003d00c. Collision and floor
    // following stay in GameplayScreen, which already has working versions of
    // both; nothing here moves the player, it only produces a direction and a
    // distance for the existing mover to consume.
    //
    // ANGLE UNITS. The original works in 4096 units per full turn, masked with
    // & 0xfff. Pitch uses the same units. Everything below is in those units;
    // conversion to radians happens once, at the boundary.
    namespace PlayerCamera
    {
        constexpr int ANGLE_UNITS = 4096;      // one full turn
        constexpr int ANGLE_MASK = ANGLE_UNITS - 1;
        constexpr int ANGLE_QUARTER = 1024;    // 0x400, 90 degrees
        constexpr int ANGLE_HALF = 2048;       // 0x800, 180 degrees
        constexpr int ANGLE_EIGHTH = 512;      // 0x200, 45 degrees

        // Trig table scale. PROVEN, not assumed: FUN_00026d1c (the 3x3
        // rotation-matrix builder) multiplies two table entries together and
        // shifts the product right by 0xc, which only normalises if one unit
        // is 0x1000. Cosine is the same table read at +0x400.
        constexpr int TRIG_ONE = 4096;         // 0x1000

        // The sine table at DAT_000a7804: 4096 signed 16-bit entries indexed
        // (angle & 0xfff) * 2. DUMPED now (Formats/OriginalTables.h): it differs
        // from a rounded ideal sine by one in 41 entries, and integer results
        // that land on a cell edge depend on it.
        inline const int16_t* SineTable() { return ALTEngine::Formats::OriginalTables::SINE; }

        inline int Sin(int angle) { return SineTable()[angle & ANGLE_MASK]; }
        inline int Cos(int angle) { return SineTable()[(angle + ANGLE_QUARTER) & ANGLE_MASK]; }

        inline float ToRadians(int angle)
        {
            return static_cast<float>(angle) * (2.0f * 3.14159265358979323846f / ANGLE_UNITS);
        }

        // ------------------------------------------------------------------
        // Tuning
        // ------------------------------------------------------------------

        // Walk/run tuning sets, swapped wholesale by the run key at the top of
        // FUN_0003efcc. Every value here is READ DIRECTLY from the
        // decompilation as an immediate store except walkTurnMax, noted below.
        struct MoveTuning
        {
            int maxSpeed;   // DAT_000b0b46
            int accel;      // DAT_000b0b48
            int decel;      // DAT_000b0b4a
            int turnMax;    // DAT_000b0b32
        };

        // CONFIRMED: 0x20. Read from the image at 0x000ace24. The guess here was
        // 40, on the assumption that walking turned at two thirds of running's
        // 0x3c; the real ratio is closer to half.
        constexpr int WALK_TURN_MAX = 0x20;

        constexpr MoveTuning WALK{ 0x78, 9, 6, WALK_TURN_MAX };
        constexpr MoveTuning RUN { 0x90, 0xc, 9, 0x3c };

        constexpr int TURN_ACCEL = 4;    // FUN_0003efcc: turn rate ramps +4/tick to turnMax
        constexpr int PITCH_ACCEL = 8;   // FUN_0003efcc: pitch rate ramps +8/tick to PITCH_RATE_MAX
        constexpr int PITCH_RECENTRE = 8;// FUN_0003d510: walks pitch home 8/tick

        // Mouse-driven movement pins speed to a flat 0x4b regardless of
        // walk/run, and only engages past a +/-5 dead zone on the accumulated
        // mouse Y. Both read directly from FUN_0003efcc.
        constexpr int MOUSE_MOVE_SPEED = 0x4b;
        constexpr int MOUSE_MOVE_DEADZONE = 6;

        // The 180-degree turn key: 0x80 subtracted from yaw for 16 consecutive
        // ticks. 0x80 * 16 == 0x800 exactly, so this is a precise half turn
        // spread over 16 ticks rather than a snap.
        constexpr int SNAP_TURN_STEP = 0x80;
        constexpr int SNAP_TURN_TICKS = 16;

        // ALL FOUR CONFIRMED. These are not data-segment initialisers after all
        // - they are written at 0x000406cc-0x0004071d, a run of register loads
        // that sets up the whole player block at game start:
        //
        //     ESI 0x30       -> DAT_000b0b42   pitch rest angle
        //     EAX 0xffffff80 -> DAT_000b0b3a   pitch minimum
        //     EDX 0x10       -> DAT_000b0b36   maximum pitch rate
        //     ECX 0          -> DAT_000b0b38   pitch rate, starts stopped
        //     EDI 0x80       -> DAT_000b0b3c   pitch maximum
        //     ESI 0x78       -> DAT_000b0b46   walk speed
        //
        // That last one is 0x78, which is the walk speed already read out of
        // FUN_0003efcc - so this is definitely the right block.
        //
        // EVERY ONE OF THE FOUR GUESSES WAS WRONG, and the pitch range is far
        // tighter than assumed: +/-0x80 is +/-11.25 degrees, not the +/-34 that
        // was here. The rest angle is 0x30 rather than level, so the view sits
        // slightly off centre, and the rate cap is 0x10 rather than 0x40 - four
        // times slower to swing.
        constexpr int PITCH_RATE_MAX = 0x10;   // DAT_000b0b36
        constexpr int PITCH_MIN = -0x80;       // DAT_000b0b3a
        constexpr int PITCH_MAX = 0x80;        // DAT_000b0b3c
        // The original's 0x30, in the port's direction. Its pitch runs the
        // other way (see RAMP_PITCH_SIGN): the camera's roll of 0x800
        // (DAT_000b0a54) puts its view space y down the screen, and the pitch
        // turns in that space, so its +0x30 tips the view DOWN a little. The
        // port's pitch is positive up, so the same rest is -0x30. It sat at
        // +0x30 here, slightly up.
        constexpr int PITCH_REST = -0x30;      // DAT_000b0b42, negated

        // Standing eye height above the player's feet: DAT_000b0b4c.
        //
        // 0x300, set by the player setup block at 0x0004078f
        // (`mov edi, 0x300 ... mov [DAT_000b0b4c], di`). The player's Y
        // (DAT_000b0a4a) is the floor height itself - FUN_0003dff0 writes
        // FUN_00027e28's result straight into it - and the eye is
        // DAT_000b0a22 = DAT_000b0a4a + DAT_000b0b4c (+ the sway term,
        // FUN_0003d00c). Every projectile leaves from 0x60 below the eye.
        //
        // 0x180 is NOT the standing height. It is where FUN_0003ee4c stops: that
        // is the DEATH sequence, which lowers the eye 0xc a tick until it
        // reaches 0x180 and then raises the "fallen" flag (DAT_000b0cc0 | 0x40)
        // - the camera ending up on the floor. Reading it as the eye height
        // briefly put the living player at crawling height (Edward, 2026).
        //
        // The port's old STAND_OFFSET 32 + EYE_HEIGHT 768 was within 32 of this.
        constexpr int EYE_HEIGHT_ORIGINAL = 0x300;   // 768, standing
        constexpr int EYE_HEIGHT_DEAD = 0x180;       // 384, where the death sequence leaves it
        constexpr int EYE_HEIGHT_RECOVERY = 0xc;     // per tick, FUN_0003ee4c (the fall when dying)

        // The original's projection. FUN_0004884c divides by depth and scales by
        // the focal length DAT_00400168, which gameplay sets to 0x100
        // (FUN_00010e38 -> FUN_0004e4e8(0x100)), around a centre of (0xa0, 0x78)
        // (FUN_0004e4d4) - i.e. a 320x240 view with square pixels. One focal
        // length for both axes, so the vertical field of view is
        // 2 * atan(120 / 256), about 50.2 degrees. Wider windows keep this
        // vertical angle and see more at the sides.
        constexpr int VIEW_FOCAL_LENGTH = 0x100;
        constexpr int VIEW_HALF_HEIGHT = 0x78;
        inline const float VIEW_FOV_Y_RADIANS =
            2.0f * std::atan(static_cast<float>(VIEW_HALF_HEIGHT) / static_cast<float>(VIEW_FOCAL_LENGTH));

        // The two ids FUN_0003d00c plays each half-stride.
        //
        // 0x2c IS THE FOOTSTEP, and it took two passes to be sure. The name
        // 5004astp at slot 0x26 reads like "a step", which is what made me swap
        // these round once; that was wrong. Three things say so:
        //
        //   - NEWSFX.BAT builds slot 0x2c's sample from shipft1.raw and
        //     shipft8.raw. SHIPFT is ship footstep.
        //   - Slot 0x2c is the only one of the two that VARIES: 0204ripl,
        //     0206ripl or 0208ripl depending on the episode, which is what a
        //     surface-dependent footstep would do. 0x26 is 5004astp on all 45
        //     levels.
        //   - It is the branch FUN_0003d00c gives the positional call to, and a
        //     footstep is the sound that needs placing in the world.
        //
        // 0x26 is therefore the special case, played only when the 0x40 flag is
        // set. What that flag means is still untraced, so the constant is named
        // after the condition rather than after a guess at the surface.
        constexpr int SFX_FOOTSTEP = 0x2c;
        constexpr int SFX_FOOTSTEP_FLAGGED = 0x26;

        // The original's logic tick rates. Its own timer interrupt runs at
        // 69.74 Hz (PIT divisor 0x42d5, installed at 0x5ab80).
        //
        // HOW THE ORIGINAL RUNS (Notes/engine-remaining-work.md, item 1). Its
        // frame waits for an even number of interrupts (FUN_0004d2c8) and then
        // runs the task list n times (FUN_00012590), counting 0x6164 down from
        // n to 1 and drawing on the last pass. With its "speed compensation",
        // which is on unless the -so switch is given, n = c/2 + 1 for a frame of
        // c interrupts. At its design point, a frame drawn in two interrupts
        // (35 fps), that is two passes a frame:
        //   - every pass: the player's movement and state, doors and lifts, so
        //     they tick at 69.74 Hz;
        //   - odd passes only (test byte [0x6164], 1): the common creature task
        //     (0x342c1) and the particles (0x2d929), so 34.87 Hz;
        //   - the last pass only (0x6164 == 1): firing (FUN_0003d5b0), the weapon
        //     animation and drawing, also 34.87 Hz.
        // The creatures are at 34.87 Hz in every mode: with -so, n is 1, and
        // pass 1 is odd.
        //
        // THE PORT. A fixed tick at the interrupt rate, TICK_HZ, for the player,
        // doors and lifts, and every other tick (CREATURE_TICK_HZ) for the rest,
        // as at the design point. Drawing is free of both, and the view is
        // smoothed between player ticks. Every per-tick constant in this file is
        // per TICK_HZ tick, as in the original.
        //
        // changes-16 ran the player at 34.87 Hz and creatures at half that,
        // which kept the original's ratio but halved every creature's speed.
        constexpr double ORIGINAL_TIMER_HZ = 1193182.0 / 17109.0;   // 69.74
        constexpr double TICK_HZ = ORIGINAL_TIMER_HZ;                // 69.74
        constexpr double CREATURE_TICK_HZ = TICK_HZ / 2.0;           // 34.87

        // Modern: Player Jumping - OURS, the original has none. The lift is a
        // short hop: an apex of 0x24^2 / 2 / 3 = 216 units, under the 256-unit
        // step, reached in 12 ticks (0.17 s). The gravity is a creature's own
        // 0xc a creature tick, which is 3 a player tick. The head keeps 0x20
        // below the ceiling.
        constexpr float JUMP_LIFT = 0x24;
        constexpr float JUMP_GRAVITY = 3;
        constexpr float JUMP_HEAD_CLEARANCE = 0x20;

        inline double FootstepsPerSecond(int speed)
        {
            return (TICK_HZ * (speed * 3.0 / 2.0)) / 2048.0;
        }

        // ------------------------------------------------------------------
        // State and input
        // ------------------------------------------------------------------

        struct Input
        {
            bool forward = false;
            bool backward = false;
            bool strafeLeft = false;
            bool strafeRight = false;
            bool turnLeft = false;
            bool turnRight = false;
            bool run = false;
            bool look = false;       // the look modifier - holds to aim the view
            bool lookUp = false;
            bool lookDown = false;
            bool turn180 = false;

            int mouseYaw = 0;        // angle units to add to yaw this tick
            int mouseForward = 0;    // accumulated mouse Y, original's DAT_00404658

            // Ticks left on the terrain-speed pickup (DAT_000b0abc). Non-zero
            // suppresses the cell 5/9 slowdown entirely.
            int terrainSpeedTicks = 0;
        };

        struct State
        {
            // Player pose.
            int yaw = 0;             // DAT_000b0a52, 0..4095
            int pitch = 0;           // DAT_000b0a50
            int moveAngle = 0;       // DAT_000b0a5a
            int speed = 0;           // DAT_000b0a62 high half

            // Rate accumulators.
            int turnRate = 0;        // DAT_000b0b34
            int pitchRate = 0;       // DAT_000b0b38

            // The 180-degree turn, DAT_000b0b40 / DAT_000b0b3e.
            bool snapTurning = false;
            int snapTicks = 0;

            // Bob phase, DAT_000b0b52 high half. Free running, 16-bit wrap.
            uint16_t bobPhase = 0;

            // Previous tick's turn keys. The original compares against the
            // previous input word (DAT_000b0ca4) so that when both turn keys
            // go down the one already held wins instead of cancelling.
            bool prevTurnLeft = false;
            bool prevTurnRight = false;

            // ---- outputs, rebuilt every tick ----
            int viewYaw = 0;         // DAT_000b0ba2
            int viewPitch = 0;       // DAT_000b0ba8
            int viewRoll = 0;        // DAT_000b0bac
            int bobOffsetY = 0;      // 0 to -64 world units, added to eye height
            int throwOffsetX = 0;    // DAT_000b0b4e: 0 to -8 world units, added to a throw's x
            bool footstep = false;   // fired this tick
            bool running = false;    // DAT_000b0ab4 bit 1
        };

        // ------------------------------------------------------------------
        // Turning and pitch
        // ------------------------------------------------------------------

        inline int Clamp(int value, int low, int high)
        {
            return value < low ? low : (value > high ? high : value);
        }

        // The ramp cell attributes that drive automatic pitch, and which way
        // each one tips the view.
        //
        // These are the same attribute values LevelLoader already special-cases
        // for slope height (0x2d..0x30 and their repeats). FUN_0003efcc groups
        // them into four families of three:
        //   0x2d 0x31 0x35  and  0x30 0x34 0x38   - ramps running along Z
        //   0x2e 0x32 0x36  and  0x2f 0x33 0x37   - ramps running along X
        // Within a family the two groups are exact opposites of each other.
        //
        // Returns -1 to pitch down, +1 to pitch up, 0 to recentre.
        inline int RampPitchDirection(uint8_t attribute, int yaw)
        {
            // The original tests `abs((short)yaw) < 0x201` for the Z families.
            // Since yaw is masked to 0..0xfff it is never negative, so the abs
            // is vestigial and the test is simply yaw <= 0x200. That makes the
            // window ASYMMETRIC - facing just under a full turn (0xe00..0xfff)
            // does NOT count as facing along the ramp, even though it is the
            // same direction. Reproduced as-is; it is the original's behaviour,
            // not a transcription slip.
            const bool alongPositiveZ = (yaw <= 0x200);
            const bool alongNegativeZ = (yaw >= 0x600 && yaw <= 0xa00);
            const bool alongPositiveX = (yaw >= 0x200 && yaw <= 0x600);
            const bool alongNegativeX = (yaw >= 0xa00 && yaw <= 0xe00);

            switch (attribute)
            {
            case 0x2d: case 0x31: case 0x35:
                if (alongPositiveZ) { return -1; }
                if (alongNegativeZ) { return +1; }
                return 0;

            case 0x30: case 0x34: case 0x38:
                if (alongPositiveZ) { return +1; }
                if (alongNegativeZ) { return -1; }
                return 0;

            case 0x2e: case 0x32: case 0x36:
                if (alongNegativeX) { return +1; }
                if (alongPositiveX) { return -1; }
                return 0;

            case 0x2f: case 0x33: case 0x37:
                if (alongNegativeX) { return -1; }
                if (alongPositiveX) { return +1; }
                return 0;

            default:
                return 0;
            }
        }

        // The sign, worked out from the original rather than guessed. The
        // original's yaw 0 looks toward +z (FUN_00026d1c / FUN_00047e84 turn
        // (0, 0, s) by yaw into x = -s sin, z = s cos; the shotgun's kick along
        // its camera yaw + 0x800 goes backwards), and the port's yaw 0 looks
        // toward -z: the port's yaw is the original's turned half round. The
        // table above is the original's, read against the port's yaw. And the
        // original's pitch runs the other way to the port's: walking up 0x31
        // facing +z (yaw within 0x200 of 0) it lowers the pitch value, which
        // must tip the view up. The two half-turns cancel, so the table read
        // against the port's yaw, at +1, tips the view up a slope as the
        // original does.
        constexpr int RAMP_PITCH_SIGN = +1;

        // FUN_0003d510. Walks pitch back toward its rest angle and kills the
        // rate accumulator.
        inline void RecentrePitch(State& state)
        {
            state.pitchRate = 0;
            if (state.pitch < PITCH_REST)
            {
                state.pitch = (state.pitch + PITCH_RECENTRE <= PITCH_REST)
                    ? state.pitch + PITCH_RECENTRE : PITCH_REST;
            }
            else if (state.pitch > PITCH_REST)
            {
                state.pitch = (PITCH_REST <= state.pitch - PITCH_RECENTRE)
                    ? state.pitch - PITCH_RECENTRE : PITCH_REST;
            }
        }

        // Shared by manual look and ramp pitch: ramp the rate up, then apply.
        inline void ApplyPitch(State& state, int direction)
        {
            if (state.pitchRate < PITCH_RATE_MAX) { state.pitchRate += PITCH_ACCEL; }
            if (state.pitchRate > PITCH_RATE_MAX) { state.pitchRate = PITCH_RATE_MAX; }
            state.pitch += direction * state.pitchRate;
            state.pitch = Clamp(state.pitch, PITCH_MIN, PITCH_MAX);
        }

        // ------------------------------------------------------------------
        // One tick
        // ------------------------------------------------------------------

        // `cellAttribute` is byte +10 of the collision cell underfoot (the
        // original reads it as *(DAT_000b0ab0 + 10)).
        // `onHazardCell` is the original's DAT_000b0ab4 bit 0x40, set by
        // FUN_0003dff0 when the cell underfoot is a damage attribute - it only
        // selects which footstep sound plays.
        // `swayEnabled` is DAT_000acea0, the Camera Sway option.
        // `freeLook` is ours, not the original's: when on, the caller drives
        // pitch from the mouse and the ramp system is skipped entirely.
        inline void Tick(State& state,
                         const Input& input,
                         uint8_t cellAttribute,
                         bool onHazardCell,
                         bool swayEnabled,
                         bool freeLook)
        {
            state.footstep = false;

            // Mouse yaw goes straight in, 1:1, before anything else
            // (FUN_0003efcc adds DAT_00404656 to yaw at the top of the tick).
            if (input.mouseYaw != 0)
            {
                state.yaw = (state.yaw + input.mouseYaw) & ANGLE_MASK;
            }

            // ---- walk / run tuning set ----
            state.running = input.run;
            const MoveTuning& tuning = input.run ? RUN : WALK;

            // ---- pitch ----
            // Manual look wins outright and skips the ramp system; the original
            // jumps straight past it. Note the original checks look-down before
            // look-up, so holding both looks down.
            if (freeLook)
            {
                // Modern free look: the caller owns pitch, nothing to do.
            }
            else if (input.look)
            {
                // Up first: FUN_0003efcc tests the key that lowers its pitch
                // value (looking up, its pitch running opposite to the port's)
                // before the other (see RAMP_PITCH_SIGN).
                if (input.lookUp) { ApplyPitch(state, +1); }
                else if (input.lookDown) { ApplyPitch(state, -1); }
            }
            else
            {
                int direction = RampPitchDirection(cellAttribute, state.yaw);
                if (direction != 0) { ApplyPitch(state, direction * RAMP_PITCH_SIGN); }
                else { RecentrePitch(state); }
            }

            // ---- 180 degree turn ----
            if (!state.snapTurning && input.turn180)
            {
                state.snapTurning = true;
                state.snapTicks = 0;
            }
            if (state.snapTurning)
            {
                state.yaw = (state.yaw - SNAP_TURN_STEP) & ANGLE_MASK;
                state.snapTicks++;
                if (state.snapTicks >= SNAP_TURN_TICKS) { state.snapTurning = false; }
            }

            // ---- turning ----
            // Opposite-key priority: a turn key is ignored if the OTHER
            // direction was already held on the previous tick.
            const bool turnLeft = input.turnLeft && !state.prevTurnRight;
            const bool turnRight = input.turnRight && !state.prevTurnLeft;
            if (turnLeft || turnRight)
            {
                if (state.turnRate < tuning.turnMax) { state.turnRate += TURN_ACCEL; }
                if (state.turnRate > tuning.turnMax) { state.turnRate = tuning.turnMax; }
                state.yaw = (turnLeft ? state.yaw - state.turnRate : state.yaw + state.turnRate) & ANGLE_MASK;
            }
            else
            {
                // Snaps to zero on release - there is no turn deceleration.
                state.turnRate = 0;
            }
            state.prevTurnLeft = input.turnLeft;
            state.prevTurnRight = input.turnRight;

            // ---- movement direction and speed ----
            // Every direction is an exact 45-degree octant off the facing, and
            // ONLY straight forward gets the full acceleration set. Everything
            // else - backward, strafe, and every diagonal - goes through the
            // halved path (FUN_0003d340 rather than FUN_0003d2b8).
            const bool strafing = input.strafeLeft || input.strafeRight;
            bool moving = true;
            bool fullSpeed = false;
            int offset = 0;

            if (strafing)
            {
                if (input.forward && input.strafeLeft)        { offset = -ANGLE_EIGHTH; }
                else if (input.forward && input.strafeRight)  { offset = +ANGLE_EIGHTH; }
                else if (input.backward && input.strafeLeft)  { offset = +0xa00; }
                else if (input.backward && input.strafeRight) { offset = +0x600; }
                else if (input.strafeLeft)                    { offset = -ANGLE_QUARTER; }
                else                                          { offset = +ANGLE_QUARTER; }
            }
            else if (input.forward)
            {
                offset = 0;
                fullSpeed = true;
            }
            else if (input.backward)
            {
                offset = ANGLE_HALF;
            }
            else if (input.mouseForward != 0)
            {
                // Mouse-driven movement: a flat speed, no acceleration curve.
                if (input.mouseForward < -5)
                {
                    offset = 0;
                    state.moveAngle = (state.yaw + offset) & ANGLE_MASK;
                    state.speed = MOUSE_MOVE_SPEED;
                    moving = false; // speed already set, skip the accel path
                }
                else if (input.mouseForward < MOUSE_MOVE_DEADZONE)
                {
                    state.speed = 0;
                    moving = false;
                }
                else
                {
                    offset = ANGLE_HALF;
                    state.moveAngle = (state.yaw + offset) & ANGLE_MASK;
                    state.speed = MOUSE_MOVE_SPEED;
                    moving = false;
                }
            }
            else
            {
                // Nothing held: decelerate to a stop, flooring at zero rather
                // than undershooting into negatives.
                moving = false;
                if (state.speed > 0)
                {
                    state.speed = (state.speed < tuning.decel) ? 0 : state.speed - tuning.decel;
                }
            }

            if (moving)
            {
                state.moveAngle = (state.yaw + offset) & ANGLE_MASK;

                // Cell attributes 5 and 9 halve movement, but only while
                // DAT_000b0abc is zero. That is a pickup-granted timer counted
                // down by FUN_0003e698, and while it runs the player crosses
                // those cells at full speed. This was marked INCOMPLETE here
                // because the suppressing state had not been found; it has now.
                const bool sluggishCell =
                    ALTEngine::Screens::TerrainEffects::CellSlowsMovement(
                        cellAttribute, input.terrainSpeedTicks);

                int accel = fullSpeed ? tuning.accel : (tuning.accel >> 1);
                int maxSpeed = fullSpeed ? tuning.maxSpeed : (tuning.maxSpeed >> 1);
                if (sluggishCell)
                {
                    accel = fullSpeed ? (tuning.accel >> 1) : (tuning.accel >> 2);
                    maxSpeed = fullSpeed ? (tuning.maxSpeed >> 1) : (tuning.maxSpeed >> 2);
                }

                state.speed += accel;
                if (state.speed > maxSpeed) { state.speed = maxSpeed; }
            }

            // ---- view pose and head bob (FUN_0003d00c) ----
            const uint16_t previousPhase = state.bobPhase;
            const int s = Sin(state.bobPhase);

            // The vertical term is min(sin(p), sin(p + 180)), which is exactly
            // -|sin(p)|. The head therefore only ever DIPS below eye level,
            // twice per cycle - it never rises above it. That asymmetry is the
            // characteristic part of the walk and is easy to get wrong by
            // reaching for a plain sine.
            const int dip = (s <= Sin(state.bobPhase + ANGLE_HALF)) ? s : Sin(state.bobPhase + ANGLE_HALF);

            state.viewRoll = s >> 9;                       // +/- 8 units, ~0.70 degrees
            // The same dip >> 9, kept whether sway is on or not, and used only
            // by the two throws, which add it to their start x (FUN_0002c4e0,
            // FUN_0002c830).
            state.throwOffsetX = dip >> 9;
            state.bobPhase = static_cast<uint16_t>(state.bobPhase + ((state.speed * 3) >> 1));

            if (!swayEnabled)
            {
                state.viewRoll = 0;
                state.viewYaw = state.yaw;
                state.bobOffsetY = 0;
            }
            else
            {
                state.viewYaw = (state.yaw + (s >> 10)) & ANGLE_MASK;  // +/- 4 units, ~0.35 degrees
                state.bobOffsetY = dip >> 6;                           // 0 to -64 world units
            }
            state.viewPitch = state.pitch;

            // Footstep on each half-cycle of the phase. Cell attribute 9
            // suppresses it entirely. Sway being off does NOT stop footsteps -
            // the original keeps them running either way.
            if (((state.bobPhase & 0x7ff) < (previousPhase & 0x7ff)) && cellAttribute != 9)
            {
                state.footstep = true;
            }
            (void)onHazardCell; // selects SFX_FOOTSTEP_HAZARD once SFX are wired
        }

        // Which sound a footstep should play this tick.
        inline int FootstepSound(bool onHazardCell)
        {
            return onHazardCell ? SFX_FOOTSTEP_FLAGGED : SFX_FOOTSTEP;
        }

        // Per-tick world-space movement for the caller's collision mover.
        // Direction is the move angle; magnitude is the speed. Matches the
        // port's existing forward convention (yaw 0 looks down -Z).
        inline void MovementStep(const State& state, float& outX, float& outZ)
        {
            outX = static_cast<float>(state.speed) * static_cast<float>(Sin(state.moveAngle)) / TRIG_ONE;
            outZ = -static_cast<float>(state.speed) * static_cast<float>(Cos(state.moveAngle)) / TRIG_ONE;
        }
    }
}
