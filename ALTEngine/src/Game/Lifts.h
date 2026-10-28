#pragma once

#include "../Formats/LevelLoader.h"

#include <cstdint>
#include <vector>

namespace ALTEngine::Game::Lifts
{
    // LIFTS - set up by FUN_000423e8, run as a timer task per lift whose
    // handler is swapped between four routines:
    //
    //   IDLE     0x42238  waits for the trigger counter to reach the threshold,
    //                     then sets off away from whichever end it is at (and,
    //                     the first time, counts itself in DAT_000b0b2a and
    //                     raises the HUD notice);
    //   ASCEND   0x420ec  floor up one height unit a tick (two with flag 0x10)
    //   DESCEND  0x41fa0  floor down the same;
    //   WAIT     0x41ed0  counts the dwell down, then sets off the other way.
    //
    // A lift is a w x h block of cells whose FLOOR BYTE it rewrites as it
    // moves (FUN_00041e80) - so anything standing there rides it, and the mover
    // and every height test see the new floor with no special case.
    //
    // Arriving at either end plays 0x5f; setting off plays 0x5e. On arrival a
    // lift that cycles on its own (record byte 8 non-zero: flag 0x40) waits and
    // goes back; one with no dwell (byte 9 zero) resets its trigger counter and
    // idles; one that started high (flag 0x20) and has a dwell waits and
    // returns; anything else resets and idles.
    //
    // Record layout (16 bytes; the loader's field names predate this):
    //   0 cell x   1 cell z   2 width   3 depth   4 low   5 high   6 -
    //   7 flags    8 auto dwell   9 dwell   10 threshold   11 counter   12 model
    // Flags: 1 at the top, 2 rising, 4 at the bottom, 8 falling, 0x10 fast,
    // 0x20 started at the top, 0x40 cycles by itself, 0x80 has run once.
    enum class Phase { Idle, Ascend, Descend, Wait };

    inline constexpr uint8_t AT_TOP = 0x01;
    inline constexpr uint8_t RISING = 0x02;
    inline constexpr uint8_t AT_BOTTOM = 0x04;
    inline constexpr uint8_t FALLING = 0x08;
    inline constexpr uint8_t FAST = 0x10;
    inline constexpr uint8_t STARTED_HIGH = 0x20;
    inline constexpr uint8_t CYCLES = 0x40;
    inline constexpr uint8_t RAN_ONCE = 0x80;

    inline constexpr int SOUND_START = 0x5e;
    inline constexpr int SOUND_STOP = 0x5f;

    struct Lift
    {
        int cellX = 0, cellZ = 0, width = 1, depth = 1;
        int low = 0, high = 0;          // floor heights, in height units (x32)
        uint8_t flags = 0;
        int dwellCounter = 0;           // +0x16
        int dwell = 0;                  // +0x17
        int threshold = 0;              // +0x18
        int counter = 0;                // +0x19
        int model = 0;                  // +0x1a
        int height = 0;                 // the floor byte it is at now
        Phase phase = Phase::Idle;

        // Grid-space centre of the platform, and its Y (height * 32 + 0x10).
        float CentreX() const { return static_cast<float>(cellX * 512 + width * 256); }
        float CentreZ() const { return static_cast<float>(cellZ * 512 + depth * 256); }
        float PlatformY() const { return static_cast<float>(height * 32 + 0x10); }
        bool Moving() const { return (flags & (RISING | FALLING)) != 0; }
    };

    // FUN_000423e8: one lift per record, its cells set to its starting floor.
    std::vector<Lift> Build(Formats::LevelGeometry& level);

    // Trigger commands 7 (FUN_00041dd8: add) and 5 (FUN_00041e20: add, but
    // only as the increment that reaches the threshold). Both refuse a lift
    // that is moving or already due to go. True means the command succeeded.
    bool AddToCounter(Lift& lift, int amount, bool finalIncrementOnly);

    struct TickResult
    {
        bool started = false;           // play SOUND_START
        bool stopped = false;           // play SOUND_STOP
        bool firstActivation = false;   // the HUD's "lift activated"
    };

    // One tick of the lift's task.
    TickResult Tick(Lift& lift, Formats::LevelGeometry& level);
}
