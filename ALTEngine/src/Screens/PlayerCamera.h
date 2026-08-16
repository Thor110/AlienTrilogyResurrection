#pragma once

#include <cmath>
#include <cstdint>

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
        // (angle & 0xfff) * 2.
        //
        // RECONSTRUCTED, NOT DUMPED. The table's index range, entry size and
        // fixed-point scale are all read from the code, so the shape is
        // certain; the entry VALUES are generated here as an ideal sine rather
        // than lifted from the binary. If the original's table is hand-tweaked
        // or uses a different rounding, small differences would show up as a
        // slightly different bob shape - dumping 8192 bytes at 0x000a7804 would
        // settle it. Nothing else in this file depends on the values being
        // exact.
        inline const int16_t* SineTable()
        {
            static const int16_t* table = []
            {
                static int16_t values[ANGLE_UNITS];
                for (int i = 0; i < ANGLE_UNITS; ++i)
                {
                    double radians = (2.0 * 3.14159265358979323846 * i) / ANGLE_UNITS;
                    values[i] = static_cast<int16_t>(std::lround(std::sin(radians) * TRIG_ONE));
                }
                return values;
            }();
            return table;
        }

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

        // DAT_000ace24 is a data-segment initialiser with no write site
        // anywhere in the decompiled export, so the walking turn cap is the one
        // value in this struct that is A GUESS. Running is 0x3c (60); this
        // assumes walking turns at two thirds of that. If turning while walking
        // feels wrong relative to running, this is the number - and the real
        // one can be read straight out of Ghidra's listing at 0x000ace24.
        constexpr int WALK_TURN_MAX_GUESS = 40;   // GUESS - real value at DAT_000ace24

        constexpr MoveTuning WALK{ 0x78, 9, 6, WALK_TURN_MAX_GUESS };
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

        // FOUR VALUES THAT COULD NOT BE TRACED. DAT_000b0b36, DAT_000b0b3a,
        // DAT_000b0b3c and DAT_000b0b42 have NO write site anywhere in the
        // decompiled export - they are initialised in the data segment. The
        // values below are GUESSES chosen to be plausible in the units the code
        // proves they are in (4096 per turn), and are the first thing to
        // replace once the real initialisers are read out of Ghidra.
        constexpr int PITCH_RATE_MAX_GUESS = 0x40;   // GUESS - real value at DAT_000b0b36
        constexpr int PITCH_MIN_GUESS = -0x180;      // GUESS - real value at DAT_000b0b3a (~ -34 degrees)
        constexpr int PITCH_MAX_GUESS = 0x180;       // GUESS - real value at DAT_000b0b3c (~ +34 degrees)
        constexpr int PITCH_REST_GUESS = 0;          // GUESS - real value at DAT_000b0b42 (level gaze)

        // Standing eye height above the player's feet. DAT_000b0b4c, set to
        // 0x180 by FUN_0003ee4c, which also eases it back down 0xc per tick
        // after it has been displaced.
        //
        // NOTE THE DISCREPANCY. GameplayScreen currently uses STAND_OFFSET +
        // EYE_HEIGHT == 800 for the same quantity, and the world scale is not
        // in question (both the original and the port put 512 units in a cell
        // and derive the cell index with >> 9). 0x180 is 384. Either the
        // original's player Y already carries part of the offset out of
        // FUN_00027e28, or one of the two numbers is wrong. Until that is
        // settled this is NOT applied - GameplayScreen keeps its own working
        // eye height and only the bob offset below is added to it.
        constexpr int EYE_HEIGHT_ORIGINAL = 0x180;   // 384 - see note, currently unused
        constexpr int EYE_HEIGHT_RECOVERY = 0xc;     // per tick, FUN_0003ee4c

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

        // The original's logic tick rate.
        //
        // THIS IS THE ONE UNRESOLVED KNOB AND IT SCALES EVERYTHING. Every
        // constant above is per-tick, so the tick rate alone decides how fast
        // the player actually moves. Nothing in the decompilation states it.
        //
        // 30 is used here for two independent reasons, neither conclusive:
        // the PC release is a port of a 30fps PSX title, and 30 puts the
        // footstep cadence at a believable ~2.6 steps/second walking (see
        // FootstepsPerSecond below) where 60 gives an implausible ~5.3.
        //
        // TO SOLVE IT PROPERLY: footsteps fire every time the bob phase
        // crosses 0x800, so
        //     steps/second = TICK_HZ * (speed * 3 / 2) / 0x800
        // Count footsteps over ten seconds of walking in a recording of the
        // original and solve for TICK_HZ. That is exact and needs no guessing.
        //
        // A TESTABLE CONSEQUENCE: if 30 is right, the motion tracker's 48-tick
        // sweep is currently running at twice the original's speed, because
        // GameplayScreen ticks it at 60. Worth watching for.
        //
        // EVIDENCE SO FAR, and it does not all point the same way:
        //   - Footstep cadence favours 30. Anything below about 25 puts walking
        //     under two steps a second, which is a stroll.
        //   - The pistol's firing animation is 3 frames at 2 ticks each, so 6
        //     ticks. At 30 that is 0.20s, which reads as slightly too fast
        //     against the original (Edward, 2026). 0.30s would be 20Hz.
        //   - The hand-tuned MOVE_SPEED this replaced was 2000 units/second,
        //     which against a walk speed of 0x78 implies about 17Hz.
        //
        // Two of the three favour something near 17-20. The footstep number is
        // the only one that can be measured EXACTLY rather than judged, which is
        // why it is still the thing to settle this:
        //     steps/second = TICK_HZ * (speed * 3 / 2) / 0x800
        // Count footsteps over ten seconds of walking in a recording of the
        // original and solve. Left at 30 until then, because changing it moves
        // movement speed by the same proportion and that is not a change to make
        // on a judgement call about an animation.
        constexpr int TICK_HZ = 30;   // GUESS - see above

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

        // If ramps pitch the view the wrong way, flip this and only this.
        //
        // The sign depends on the original's yaw zero pointing the same way as
        // the port's (yaw 0 looks down -Z). Both derive from the same level
        // data so they should agree, but that alignment is ASSUMED, not proven
        // - it is the one place in this file where a wrong guess produces a
        // clean, obvious, single-symptom failure. Walking up a ramp and seeing
        // the view tip down instead of up means this should be -1.
        constexpr int RAMP_PITCH_SIGN = +1;   // GUESS - verify by walking a ramp

        // FUN_0003d510. Walks pitch back toward its rest angle and kills the
        // rate accumulator.
        inline void RecentrePitch(State& state)
        {
            state.pitchRate = 0;
            if (state.pitch < PITCH_REST_GUESS)
            {
                state.pitch = (state.pitch + PITCH_RECENTRE <= PITCH_REST_GUESS)
                    ? state.pitch + PITCH_RECENTRE : PITCH_REST_GUESS;
            }
            else if (state.pitch > PITCH_REST_GUESS)
            {
                state.pitch = (PITCH_REST_GUESS <= state.pitch - PITCH_RECENTRE)
                    ? state.pitch - PITCH_RECENTRE : PITCH_REST_GUESS;
            }
        }

        // Shared by manual look and ramp pitch: ramp the rate up, then apply.
        inline void ApplyPitch(State& state, int direction)
        {
            if (state.pitchRate < PITCH_RATE_MAX_GUESS) { state.pitchRate += PITCH_ACCEL; }
            if (state.pitchRate > PITCH_RATE_MAX_GUESS) { state.pitchRate = PITCH_RATE_MAX_GUESS; }
            state.pitch += direction * state.pitchRate;
            state.pitch = Clamp(state.pitch, PITCH_MIN_GUESS, PITCH_MAX_GUESS);
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
                if (input.lookDown) { ApplyPitch(state, -1); }
                else if (input.lookUp) { ApplyPitch(state, +1); }
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

                // Cell attributes 5 and 9 halve movement again - the original
                // gates this on DAT_000b0abc being clear as well, which is a
                // state flag not yet traced, so this applies it unconditionally.
                // MARKED AS INCOMPLETE: if some state is supposed to suppress
                // the slowdown, that is the missing piece.
                const bool sluggishCell = (cellAttribute == 5 || cellAttribute == 9);

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
