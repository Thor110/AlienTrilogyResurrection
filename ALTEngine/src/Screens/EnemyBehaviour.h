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
        // Not fixed here, and NOT worked around either: if the original does not
        // drop them, neither should this, until we know whether that is a bug in
        // the game or a condition we have not found.
        inline constexpr bool CEILING_DROP_IMPLEMENTED = false;
    }
}
