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
    // idles; any other waits, goes back to where it started and idles there.
    //
    // Record layout (16 bytes; the loader's field names predate this):
    //   0 cell x   1 cell z   2 width   3 depth   4 low   5 high   6 -
    //   7 flags    8 auto dwell   9 dwell   10 threshold   11 counter   12 model
    // On disc "low" is 0 and "high" is the travel: as the level loads,
    // FUN_00041b30 sets low to the floor of the lift's first cell and adds
    // that floor to high.
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
    // FUN_00040c38's mode for each: a lift setting off after its wait and an
    // arrival are mode 0, one the player's trigger set off is mode 3.
    inline constexpr int SOUND_MODE = 0;
    inline constexpr int SOUND_MODE_TRIGGERED = 3;

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

        // Where the platform mesh is drawn, in grid space: the centre of the
        // lift's FIRST cell (record words 0 and 4, x * 512 + 0x100), as
        // FUN_00041ba4 hands the record itself to the transform. The centre
        // of the whole block (words 0x1c/0x20) is only for its visibility
        // test. Y is height * 32 + 0x10.
        float DrawX() const { return static_cast<float>(cellX * 512 + 256); }
        float DrawZ() const { return static_cast<float>(cellZ * 512 + 256); }
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
        bool startedByTrigger = false;  // set off by its counter (mode 3, louder) rather than after a wait (mode 0)
        bool stopped = false;           // play SOUND_STOP
        bool firstActivation = false;   // the HUD's "lift activated"
    };

    // One tick of the lift's task.
    TickResult Tick(Lift& lift, Formats::LevelGeometry& level);
}
