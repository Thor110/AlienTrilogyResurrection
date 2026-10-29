#pragma once

#include <string>

namespace ALTEngine::Game::ResupplyTimer
{
    // The resupply (bonus) levels' countdown: 113, 114, 115, 155, 242, 243,
    // 352 and 353, the campaign indices in the bonus table at 0xa616e.
    //
    // FUN_0003b518 starts it with the level: for a bonus level it adds a task
    // (0x3b2e8) holding the seconds left, 30 for index 9 (155), 45 for 27 and
    // 30 (352, 353), 60 for the rest, and sets the real-time word 0x1a7f68 to
    // 0x46, which the timer interrupt counts down.
    //
    // The task runs every pass. Each time it finds the word at zero it reloads
    // it (so a second is 0x46 interrupts) and takes a second off. On a second
    // that changes it may sound a cue (FUN_00040b2c, full volume): at 20 and 17
    // seconds the level's own (0x64 for 115, 155, 242, 243; 0x65 for 113, 114;
    // 0x62 for 352, 353), at 14 seconds 0x63 (0x62 past index 21), and at 11,
    // 8, 5 and 2 seconds 0x62 past index 21. It prints the seconds as two
    // digits in the HUD font at (16, 19) (FUN_00043720), steady above 17 and
    // blinking on bit 3 of its pass count from 17 down, and at zero sets the
    // level-done bit (0x10cc0 |= 4), the same bit the end-of-level trigger
    // sets, which takes the player to the mission assessment.
    inline constexpr int INTERRUPTS_PER_SECOND = 0x46;
    inline constexpr int BLINK_FROM = 17;   // 0x11
    inline constexpr int HUD_X = 0x10;
    inline constexpr int HUD_Y = 0x13;

    struct State
    {
        bool active = false;
        int campaignIndex = -1;
        int seconds = 0;          // task +0x0c
        int interruptsLeft = 0;   // 0x1a7f68
        unsigned passes = 0;      // task +0x14
        bool expired = false;
    };

    inline bool IsResupplyLevel(int campaignIndex)
    {
        switch (campaignIndex)
        {
        case 2: case 5: case 7: case 9: case 16: case 19: case 27: case 30: return true;
        default: return false;
        }
    }

    // FUN_0003b518.
    inline State Start(int campaignIndex)
    {
        State s;
        if (!IsResupplyLevel(campaignIndex)) { return s; }
        s.active = true;
        s.campaignIndex = campaignIndex;
        s.seconds = (campaignIndex == 9) ? 0x1e : (campaignIndex == 27 || campaignIndex == 30) ? 0x2d : 0x3c;
        s.interruptsLeft = INTERRUPTS_PER_SECOND;
        return s;
    }

    // The cue for a second that has just changed, or -1 (0x3b368).
    inline int CueFor(int seconds, int campaignIndex)
    {
        const bool late = campaignIndex > 0x15;
        switch (seconds)
        {
        case 20: case 17:
            switch (campaignIndex)
            {
            case 7: case 9: case 16: case 19: return 0x64;
            case 2: case 5: return 0x65;
            case 27: case 30: return 0x62;
            default: return -1;
            }
        case 14: return late ? 0x62 : 0x63;
        case 11: case 8: case 5: case 2: return late ? 0x62 : -1;
        default: return -1;
        }
    }

    // One pass of the task. `interrupts` is how many timer interrupts have
    // passed since the last one. Returns the cue to play this pass, or -1.
    inline int Pass(State& s, int interrupts)
    {
        if (!s.active) { return -1; }
        s.passes++;
        int cue = -1;
        // The interrupt only counts the word down to zero; the task reloads it.
        s.interruptsLeft = (interrupts >= s.interruptsLeft) ? 0 : s.interruptsLeft - interrupts;
        if (s.interruptsLeft == 0)
        {
            s.interruptsLeft = INTERRUPTS_PER_SECOND;
            if (s.seconds > 0)
            {
                s.seconds--;
                cue = CueFor(s.seconds, s.campaignIndex);
            }
        }
        if (s.seconds <= 0) { s.expired = true; }
        return cue;
    }

    // The two digits the HUD shows.
    inline std::string Text(const State& s)
    {
        const int v = s.seconds < 0 ? 0 : s.seconds % 100;
        return std::string{ static_cast<char>('0' + v / 10), static_cast<char>('0' + v % 10) };
    }

    // From 17 seconds down the digits blink on bit 3 of the pass count.
    inline bool Blinking(const State& s) { return s.seconds <= BLINK_FROM; }
    inline bool BlinkPhase(const State& s) { return (s.passes & 8u) != 0; }
}
