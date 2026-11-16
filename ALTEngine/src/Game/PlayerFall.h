#pragma once

#include <cstdlib>

namespace ALTEngine::Game::PlayerFall
{
    // THE PLAYER'S HEIGHT, AS THE ORIGINAL'S PLAYER TASK KEEPS IT.
    //
    // The player's task (0x40040) is one of three, swapped as the floor under
    // the player changes:
    //
    //   WALK  (0x4007b) the input, the move (FUN_0003dff0, which sets Y to the
    //                   floor under the new position), then: Y was above that
    //                   floor -> FALL toward it, the fall starting at the old Y
    //                   (+0xc); below it -> CLIMB up to it. Either way Y is put
    //                   back to where it was: it gets there a step a pass.
    //   FALL  (0x3eafc) Y down 0x100 a pass. Stepping past the floor puts Y on
    //                   it and the fall's start with it, so that landing hurts
    //                   nothing (0x3eb66); arriving on it exactly hurts by the
    //                   height fallen (0x3ebaa), then back to WALK. A landing
    //                   on a cell of attribute 8 or 9 sounds 0x6b.
    //   CLIMB (0x3ed70) Y up 0x100 a pass, held at the floor; there, WALK.
    //
    // FALL and CLIMB move too (they call FUN_0003dff0 after their own step)
    // and choose again after it, aiming at the new floor; only WALK sets a
    // fall's start, so a fall that lands and goes on falling in the same pass
    // keeps its first start.
    //
    // A dying player's task (FUN_0003ee4c) calls none of this: Y stays put.
    enum class Task { Walk, Fall, Climb };

    inline constexpr int STEP = 0x100;
    inline constexpr int LANDING_SOUND = 0x6b;

    struct State
    {
        Task task = Task::Walk;
        int target = 0;   // +8: the floor it is falling or climbing to
        int start = 0;    // +0xc: the height the fall began at
    };

    // The task's own step on Y, before the move. Returns the height of a fall
    // that has just landed (0 for anything else).
    inline int StepBeforeMove(State& s, int& y)
    {
        if (s.task == Task::Fall)
        {
            if (s.target < y)
            {
                y -= STEP;
                if (s.target > y)
                {
                    // Past the floor: put on it, and the fall measures nothing.
                    if (std::abs(y - s.target) <= STEP) { s.start = s.target; }
                    y = s.target;
                }
            }
            if (s.target == y)
            {
                s.task = Task::Walk;
                return std::abs(s.start - y);
            }
            return 0;
        }
        if (s.task == Task::Climb)
        {
            if (s.target > y)
            {
                y += STEP;
                if (s.target < y) { y = s.target; }
            }
            if (s.target == y) { s.task = Task::Walk; }
        }
        return 0;
    }

    // After the move: `y` is the height before it, `floor` the floor under the
    // new position, `walked` whether this pass ran the WALK task (the only one
    // that sets a fall's start).
    inline void AfterMove(State& s, int& y, int floor, bool walked)
    {
        if (y > floor)
        {
            if (walked) { s.start = y; }
            s.target = floor;
            s.task = Task::Fall;
        }
        else if (y < floor)
        {
            s.target = floor;
            s.task = Task::Climb;
        }
    }

    // What a landing does to health (0x3ebb8-0x3ec5f): over 0xa00 all of it,
    // over 0x800 0x19, over 0x600 0xf, over 0x400 5, else nothing. -1 is "all".
    inline int LandingDamage(int drop)
    {
        if (drop > 0xa00) { return -1; }
        if (drop > 0x800) { return 0x19; }
        if (drop > 0x600) { return 0xf; }
        if (drop > 0x400) { return 5; }
        return 0;
    }
}
