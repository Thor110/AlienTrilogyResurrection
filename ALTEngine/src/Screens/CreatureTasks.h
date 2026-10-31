#pragma once

namespace ALTEngine::Screens::CreatureTasks
{
    // THE EGG - LAB_000356a0, the task FUN_0002e638 registers for type 1.
    //
    // Spawn (FUN_0002e638's type 1 arm): state 0, the +0x52 timer 0x20, the
    // +0x4e timer 0x40, animation 0, and its own distance thresholds - 0x800
    // (+0x4a) and 0xc00 (+0x48) - in place of the record's. Its record's byte 6
    // (+0x72) is not a drop: it is the monster it holds.
    //
    // Each tick both timers count down, then by state (+0x6f, table 0x3567c):
    //   0 CLOSED   0x35716  once +0x52 is clear and the closed animation has
    //                       played through: CHECK.
    //   2 CHECK    0x35737  the player inside the middle band (mode < 3): start
    //                       opening (animation 2), phase 1, OPENING. Otherwise
    //                       restart the closed animation and wait 0x40-0xbf
    //                       ticks in CLOSED.
    //   3 OPENING  0x3577d  when the animation ends: phase 2, OPEN.
    //   4 OPEN     0x3579a  the player inside the near band (mode < 2) with a
    //                       clear line (FUN_00032aec): let the hugger out
    //                       (FUN_00035530, animation 10 if there was one), phase
    //                       3, and the egg dies (FUN_00030b04).
    //   6 HIT      0x357ea  after a hit: back to OPENING if it was opening; a
    //                       closed egg starts opening; an open one is OPEN again.
    //   7/8               dying and dead, the damage system's.
    // The task's phase (its own +0xc) remembers how far the egg has got.
    namespace Egg
    {
        enum State { CLOSED = 0, CHECK = 2, OPENING = 3, OPEN = 4 };
        enum Phase { PHASE_CLOSED = 0, PHASE_OPENING = 1, PHASE_OPEN = 2, PHASE_RELEASED = 3 };

        inline constexpr int NEAR_THRESHOLD = 0x800;
        inline constexpr int MIDDLE_THRESHOLD = 0xc00;
        inline constexpr int SPAWN_IDLE_TIMER = 0x20;
        inline constexpr int SPAWN_HIT_TIMER = 0x40;
        inline constexpr int RECHECK_MIN = 0x40;     // + (random & 0x7f)
        inline constexpr int ANIM_CLOSED = 0;
        inline constexpr int ANIM_OPENING = 2;
        inline constexpr int ANIM_RELEASE = 10;

        struct Egg
        {
            int state = CLOSED;
            int phase = PHASE_CLOSED;
            int idleTimer = SPAWN_IDLE_TIMER;   // +0x52
            int hitTimer = SPAWN_HIT_TIMER;     // +0x4e
        };

        struct Input
        {
            int mode = 4;                 // the distance band, egg thresholds
            bool lineToPlayer = false;    // FUN_00032aec
            bool animationEnded = false;  // +0xb6 bit 2
            bool recoveredFromHit = false;
            int random = 0;               // FUN_000100ec
        };

        struct Output
        {
            int startAnimation = -1;      // FUN_0002f4b0, restarting it
            bool release = false;         // FUN_00035530 with the held monster
            bool die = false;             // FUN_00030b04
        };

        inline Output Step(Egg& egg, const Input& in)
        {
            Output out;
            if (egg.hitTimer > 0) { egg.hitTimer--; }
            if (egg.idleTimer > 0) { egg.idleTimer--; }

            if (in.recoveredFromHit)
            {
                if (egg.phase == PHASE_CLOSED)
                {
                    out.startAnimation = ANIM_OPENING;
                    egg.phase = PHASE_OPENING;
                    egg.state = OPENING;
                }
                else if (egg.phase == PHASE_OPENING) { egg.state = OPENING; }
                else if (egg.phase == PHASE_OPEN) { egg.state = OPEN; }
                return out;
            }

            switch (egg.state)
            {
            case CLOSED:
                if (egg.idleTimer == 0 && in.animationEnded) { egg.state = CHECK; }
                break;

            case CHECK:
                if (in.mode < 3)
                {
                    out.startAnimation = ANIM_OPENING;
                    egg.phase = PHASE_OPENING;
                    egg.state = OPENING;
                }
                else
                {
                    out.startAnimation = ANIM_CLOSED;
                    egg.state = CLOSED;
                    egg.idleTimer = (in.random & 0x7f) + RECHECK_MIN;
                }
                break;

            case OPENING:
                if (in.animationEnded)
                {
                    egg.phase = PHASE_OPEN;
                    egg.state = OPEN;
                }
                break;

            case OPEN:
                if (in.mode < 2 && in.lineToPlayer)
                {
                    out.release = true;
                    egg.phase = PHASE_RELEASED;
                    out.die = true;
                }
                break;

            default:
                break;
            }
            return out;
        }
    }

    // THE GUNNERS - types 0x0b-0x0e (guard, soldier, synthetic, handler).
    // FUN_00033ff8 has no arm for them: they do no harm at the end of the
    // attack. Their harm is a round fired part-way through it, when the
    // attack animation's script raises +0xb6 bit 3 (0x34892) - FUN_0002af8c,
    // or the lobbed grenade FUN_0002b240 when +0x6c bit 0 is set (nothing
    // traced sets it). The round leaves the creature 0x2c0 up (a guard) or
    // 0x240 (the others), aimed at a point 0x280 above the player's feet, at
    // 0x80 a tick; it lives 0x40 ticks and does 1 damage with cause 3.
    namespace Gunner
    {
        inline constexpr bool IsGunner(int type) { return type >= 0x0b && type <= 0x0e; }
        inline constexpr int LaunchHeight(int type) { return type == 0x0b ? 0x2c0 : 0x240; }
        inline constexpr int AIM_HEIGHT = 0x280;
        inline constexpr int SPEED = 0x80;
    }

    // What FUN_00035530 does to the monster it lets out: it stands in the
    // egg's cell, faces the way the egg sees the player (+0x54), is already
    // alerted (+0x6c bit 2) with no idle wait, and is launched into the act
    // state (5) mid-leap - 0x40 along its heading (FUN_00032f78) and 0x60 up.
    namespace EggRelease
    {
        inline constexpr int SPEED = 0x40;
        inline constexpr int LIFT = 0x60;
    }

    // THE COLONIST - LAB_000364d4, type 10: a captive cocooned in the wall.
    // It never moves. It turns its head to follow the player, cries out, and
    // when the player comes close in front of it the chestburster inside it
    // breaks out.
    //
    // Spawn (FUN_0002e638's type 10 arm): pushed 0x40 out along its facing
    // octant (record byte 4, +0x6d), stood 0x40 over the floor, animation 0xc,
    // and +0x6e (the last view it showed) 0xff. Its record's speed word
    // (+0x5e) is the monster inside it, and byte 6 (+0x72) a pickup it drops.
    //
    // Each tick, after both timers: which eighth of the way round the player
    // is, against its own facing - (facing octant - ((bearing + 0x900) >> 9))
    // & 7, 4 being straight in front. Then by state (+0x6f, table 0x364b0):
    //   0 LOOKING   0x3655d  with +0x52 running: show the look for that
    //                        eighth (animation 8 + it) whenever it changes.
    //                        With +0x52 clear: the player in the near band and
    //                        straight in front starts the burst (animation
    //                        0x1a, BURSTING, wait 0x28-0xa7); in the middle band
    //                        it plays 0x18 if in front, or cries out (sound
    //                        0x6e), and waits 0x78-0xf7 in CRYING.
    //   2 CRYING    0x365f4  back to LOOKING when the animation's event fires
    //                        or the player leaves the front.
    //   3 BURSTING  0x3661f  on the event: the burster comes out
    //                        (FUN_00035624), its pickup drops (FUN_0003bc9c),
    //                        and the colonist is BURST. The player leaving the
    //                        front first puts it back to LOOKING.
    //   4 BURST     0x36649  shows the empty cocoon's look (animation 0-7) as
    //                        the player moves round.
    //   6 HIT       0x3665b  on the event: LOOKING, with 8 ticks to wait.
    //   7/8               dying (animation 0x19), then dead: 0x1b plus the
    //                     eighth, following the player round (0x3667f).
    // The animation "event" (+0xb6 bit 1) comes from the animation scripts,
    // which the port does not run; an animation playing through stands in.
    namespace Colonist
    {
        enum State { LOOKING = 0, CRYING = 2, BURSTING = 3, BURST = 4 };

        inline constexpr int FRONT = 4;
        inline constexpr int SPAWN_PUSH = 0x40;
        inline constexpr int SPAWN_LIFT = 0x40;
        inline constexpr int SPAWN_ANIMATION = 0x0c;
        inline constexpr int ANIM_LOOK = 8;           // + the eighth
        inline constexpr int ANIM_BURST_LOOK = 0;     // + the eighth
        inline constexpr int ANIM_FRONT_CRY = 0x18;
        inline constexpr int ANIM_BURSTING = 0x1a;
        inline constexpr int ANIM_BODY = 0x1b;        // + the eighth, once dead
        inline constexpr int SOUND_CRY = 0x6e;
        inline constexpr int BURST_WAIT_MIN = 0x28;
        inline constexpr int CRY_WAIT_MIN = 0x78;
        inline constexpr int HIT_WAIT = 8;
        // What the chestburster gets on the way out (FUN_00035624).
        inline constexpr int BURSTER_SPEED = 0x60;
        inline constexpr int BURSTER_LIFT = 0x78;
        inline constexpr int BURSTER_ANIMATION = 0x0d;

        // The spawn push for each facing octant (the table at 0x2e618), in
        // grid x / z.
        inline constexpr int PUSH_X[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
        inline constexpr int PUSH_Z[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };

        inline constexpr int ViewOf(int facingOctant, int bearing)
        {
            return (facingOctant - (((bearing + 0x900) >> 9) & 7)) & 7;
        }

        struct Colonist
        {
            int state = LOOKING;
            int lastView = 0xff;      // +0x6e
            int idleTimer = 0x20;     // +0x52
        };

        struct Input
        {
            int mode = 4;
            int view = 0;
            bool animationEvent = false;
            bool recoveredFromHit = false;
            int random = 0;
        };

        struct Output
        {
            int startAnimation = -1;
            bool cry = false;
            bool burst = false;
        };

        inline Output Step(Colonist& c, const Input& in)
        {
            Output out;
            if (in.mode == 4) { return out; }
            if (c.idleTimer > 0) { c.idleTimer--; }

            auto look = [&]() {
                out.startAnimation = ANIM_LOOK + in.view;
                c.state = LOOKING;
            };

            if (in.recoveredFromHit)
            {
                look();
                c.idleTimer = HIT_WAIT;
                return out;
            }

            switch (c.state)
            {
            case LOOKING:
                if (c.idleTimer != 0)
                {
                    if (in.view != c.lastView)
                    {
                        c.lastView = in.view;
                        out.startAnimation = ANIM_LOOK + in.view;
                    }
                }
                else if (in.mode <= 1 && in.view == FRONT)
                {
                    out.startAnimation = ANIM_BURSTING;
                    c.state = BURSTING;
                    c.idleTimer = (in.random & 0x7f) + BURST_WAIT_MIN;
                }
                else if (in.mode <= 2)
                {
                    if (in.view == FRONT) { out.startAnimation = ANIM_FRONT_CRY; }
                    else { out.cry = true; }
                    c.state = CRYING;
                    c.idleTimer = (in.random & 0x7f) + CRY_WAIT_MIN;
                }
                break;

            case CRYING:
                if (in.animationEvent || in.view != FRONT) { look(); }
                break;

            case BURSTING:
                if (in.animationEvent)
                {
                    c.state = BURST;
                    out.burst = true;
                }
                else if (in.view != FRONT) { look(); }
                break;

            case BURST:
                if (in.view != c.lastView)
                {
                    c.lastView = in.view;
                    out.startAnimation = ANIM_BURST_LOOK + in.view;
                }
                break;

            default:
                break;
            }
            return out;
        }
    }

    // THE VENTS - types 0x10-0x13, steam and flame jets. Their tasks
    // (LAB_00035aa0 0x10, 0x35ae8 0x11, 0x35b68 0x12, 0x35b28 0x13) all call
    // FUN_000358a4 with the jet's particle type, emit rate and speed, and stop
    // the vent's looping sound (FUN_00040aec) when it returns 0.
    //
    // Spawn (FUN_0002e638): they face the other way to their record
    // (+0x800), and the record's byte 6 lifts them - a horizontal vent by
    // byte 6 * 32 over its usual height, a vertical one by byte 6 * 32 off the
    // floor, or, with byte 6 negative, (10 - byte 6) * 32 down from the
    // ceiling, and then it jets downward. Their fields are the record's
    // thresholds and speed word, read as a timing cycle:
    //   +0x48 (bytes 16/17)  cycle length
    //   +0x4a (bytes 18/19)  where in the cycle the jet stops
    //   +0x5e (bytes 14/15)  how long it stays stopped
    //
    // FUN_000358a4, each tick:
    //   - the cycle counter (task +0x14) counts up and wraps past the length;
    //   - the vent's trigger count below its threshold switches it off for
    //     good: once only, it sets +0x6c bit 0x10, counts it (DAT_000b0ae6 per
    //     type) and posts the steam / flame notice (FUN_0003b1f0);
    //   - nothing while its cell's zones are out of view (DAT_000b0cf0) or the
    //     player is over 0x1e00 away;
    //   - state 0 is dormant while the vent still has health - a pipe that
    //     bursts when shot (state 7: an explosion, FUN_0002aaf0, then 8). A
    //     vent with none goes straight to 8;
    //   - state 8 jets except while the cycle is in its stopped stretch, keeping
    //     its loop sound (0x68 steam, 0x1c flame) and putting out a particle
    //     every `rate` + 1 ticks (FUN_0002b0e4).
    namespace Vent
    {
        enum State { DORMANT = 0, JETTING = 8 };

        inline constexpr int SIMULATION_CUTOFF = 0x1e00;
        inline constexpr int SOUND_STEAM = 0x68;
        inline constexpr int SOUND_FLAME = 0x1c;
        inline constexpr int PARTICLE_LIFE = 0x20;

        struct Params
        {
            int particleType = 0;
            int rate = 0;
            int speed = 0;
            bool steam = false;
        };

        inline constexpr Params For(int type)
        {
            switch (type)
            {
            case 0x10: return { 0x0e, 3, 0x3c, true };
            case 0x11: return { 0x10, 2, 0x78, false };
            case 0x12: return { 0x12, 2, 0x41, true };
            case 0x13: return { 0x11, 2, 0x5a, false };
            default:   return {};
            }
        }

        inline constexpr bool IsVertical(int type) { return type == 0x12 || type == 0x13; }

        struct Vent
        {
            int state = DORMANT;
            int cycle = 0;            // task +0x14
            int emitCounter = 0;      // task +0xc
            bool soundOn = false;     // task +0x10 holds a sound
            bool switchedOff = false; // +0x6c bit 0x10
        };

        struct Input
        {
            int type = 0x10;
            int cycleLength = 0;      // +0x48
            int stopAt = 0;           // +0x4a
            int stopLength = 0;       // +0x5e
            bool triggeredOff = false;   // record byte 7 < byte 8
            bool visible = true;
            int distance = 0;
            bool hasHealth = false;   // +0x58 > 0
            bool burst = false;       // destroyed this tick (state 7)
        };

        struct Output
        {
            bool emit = false;
            bool jetting = false;     // FUN_000358a4's -1: keep the sound
            bool startSound = false;
            bool stopSound = false;   // the task's FUN_00040aec: the jet's sound ends
            bool switchOffNotice = false;
            bool explode = false;
        };

        inline Output StepEmitter(Vent& v, const Input& in);

        // One tick of the task: FUN_000358a4, and the task's own stop of the
        // loop sound whenever that comes back 0.
        inline Output Step(Vent& v, const Input& in)
        {
            const bool wasOn = v.soundOn;
            Output out = StepEmitter(v, in);
            if (!out.jetting) { v.soundOn = false; }
            // Off whichever way it went: the jet's quiet stretch, out of view
            // or range, switched off, or knocked back to dormant by a hit (the
            // original loses the sound's handle that way and leaves it playing).
            out.stopSound = wasOn && !v.soundOn;
            return out;
        }

        inline Output StepEmitter(Vent& v, const Input& in)
        {
            Output out;
            if (++v.cycle > in.cycleLength) { v.cycle = 0; }

            if (in.triggeredOff)
            {
                if (!v.switchedOff) { v.switchedOff = true; out.switchOffNotice = true; }
                return out;
            }
            if (!in.visible || in.distance > SIMULATION_CUTOFF) { return out; }

            if (in.burst)
            {
                out.explode = true;
                v.state = JETTING;
                return out;
            }
            if (v.state == DORMANT)
            {
                if (!in.hasHealth)
                {
                    v.emitCounter = 0;
                    v.cycle = 0;
                    v.soundOn = false;
                    v.state = JETTING;
                }
                return out;
            }

            const int phase = v.cycle - in.stopAt;
            if (phase >= 0 && phase <= in.stopLength) { return out; }

            out.jetting = true;
            if (!v.soundOn) { v.soundOn = true; out.startSound = true; }
            if (++v.emitCounter > For(in.type).rate)
            {
                v.emitCounter = 0;
                out.emit = true;
            }
            return out;
        }
    }
}
