#pragma once

#include "../Game/OriginalMath.h"

#include <cstdint>
#include <functional>

namespace ALTEngine::Screens::CreatureAI
{
    // THE COMMON CREATURE TASK - LAB_000342b4, the one FUN_0002e638 registers
    // for creature types 2-6, 8, 9 and 0x0b-0x0e. Its state byte (+0x6f) runs
    // through a jump table at 0x34270; states 6-8 (hit, dying, the special
    // far-away arm) are the damage system's and are not handled here.
    //
    //   0 IDLE    0x343fe  face the player outright; once the +0x52 timer has
    //                      run out, go to CHASE.
    //   2 CHASE   0x3445e  choose what to do next:
    //                        - retreating (after a hugger's leap): a waypoint
    //                          AWAY from the player down the flow field
    //                          (FUN_000337f4), or stop retreating;
    //                        - alerted: a point near the player with a clear
    //                          line to it (FUN_00032fcc), else the next node
    //                          TOWARD the player (FUN_000333c8), else a side-step
    //                          ahead (FUN_00033ad0) - each one means MOVE;
    //                        - not yet alerted: notice the player (FUN_00033a70)
    //                          and become alerted; or, seeing the player inside
    //                          the cone, ATTACK at contact range or become
    //                          alerted from further off.
    //   3 MOVE    0x34586  walk to the waypoint, turning at the creature's own
    //                      rate and only advancing when facing it exactly and
    //                      with a clear line to it. Arriving, or the player
    //                      coming into reach with the attack timer clear, means
    //                      ATTACK; losing the line means IDLE for 8 ticks.
    //   4 ATTACK  0x3476e  turn to the player; facing exactly, act
    //                      (FUN_00033cbc). Out of reach, out of line or too far
    //                      (mode 3+): back to CHASE.
    //   5 ACT     0x34874  the attack itself plays out (the leap, or the
    //                      attack animation). When it ends the hit is resolved
    //                      (FUN_00033ff8) and then: a hugger retreats; anything
    //                      else attacks again if it still can, or moves on to a
    //                      fresh point near the player, or idles for 4 ticks.
    //
    // THE QUEEN (type 7) runs her own task, LAB_00035bec, which is this one
    // with five differences - `Agent::queen` selects them:
    //   - IDLE first plays her intro, a phase in the task's own +0xc: with the
    //     wait clear and the player inside the middle band she starts
    //     animation 9 (phase 1); when that plays out, animation 10 if the
    //     player is in the near band (phase 2) or else animation 8 and 0x80
    //     ticks more (phase 0); phase 2 playing out is phase 3, and only then
    //     does IDLE go on to CHASE once the wait is clear.
    //   - CHASE never attacks. Unalerted, she becomes alerted by noticing the
    //     player, or by the player being in a zone she can see and either at
    //     contact range or in a clear line.
    //   - MOVE and ATTACK ignore the vertical reach; ATTACK acts at contact
    //     range outright, and from the middle bands with a clear line.
    //   - ACT goes again at contact range without the vertical and line tests.
    //   - A hit (state 6) before her intro is done rises at once (animation
    //     10, 0x40 ticks); after it, she attacks again if she can, or side-
    //     steps (FUN_00033ad0) with the attack timer at 0x3c, or idles 8 ticks.
    enum State
    {
        IDLE = 0,
        CHASE = 2,
        MOVE = 3,
        ATTACK = 4,
        ACT = 5,
    };

    // Spawn values (FUN_0002e638).
    inline constexpr int SPAWN_IDLE_TIMER = 0x20;     // +0x52
    inline constexpr int TURN_RATE = 0x200;           // +0x46, per tick
    inline constexpr int HUGGER_SPEED_BONUS = 0x14;   // type 2's +0x5e

    // Timers the states set.
    inline constexpr int ATTACK_TIMER = 0x3c;         // +0x68, on acting
    inline constexpr int WANDER_ATTACK_TIMER = 0x1e;  // +0x68, after an attack, moving on
    inline constexpr int LOST_LINE_IDLE = 8;          // +0x52, MOVE losing its line
    inline constexpr int AFTER_ACT_IDLE = 4;          // +0x52, ACT with nothing to do

    // Reach. The vertical separation an attack needs: 0x400, or 0x1000 for a
    // creature with +0x6c bit 8 (a ceiling crawler).
    inline constexpr int REACH = 0x400;
    inline constexpr int REACH_CEILING = 0x1000;
    inline constexpr int CONTACT_VERTICAL = 0x600;

    // Waypoint pickers' distances.
    inline constexpr int NEAR_PLAYER_MIN = 0x400;     // FUN_00032fcc: 0x400 + (rand & 0x1ff)
    inline constexpr int NODE_SCATTER_MIN = 0x200;    // FUN_000337f4/333c8: 0x200 + (rand & 0x1ff)
    inline constexpr int SIDESTEP = 0x400;            // FUN_00033ad0
    inline constexpr int SIDESTEP_SPREAD = 0x200;     // +-, around straight ahead
    inline constexpr int PICK_TRIES = 4;

    // What the AI needs to ask of the level. Positions are in grid space
    // (the original's own coordinates, as FindFloorHeightGridSpace takes).
    struct World
    {
        // The cell is walkable (attribute < 0x14 or > 0x2c) and empty, or holds
        // the player (FUN_00032fcc and the node pickers accept 0xff).
        std::function<bool(int x, int z)> cellOpenOrPlayer;
        // Walkable and empty (FUN_00033ad0).
        std::function<bool(int x, int z)> cellOpen;
        // FUN_0003231c from the creature to a point.
        std::function<bool(int x, int z, int distance)> lineTo;
        // The flow field (FUN_000281cc): the node a creature in this cell
        // belongs to, the neighbour a step TOWARD / AWAY from the player, and a
        // node's position. -1 for none.
        std::function<int(int x, int z)> nodeAt;
        std::function<int(int node)> nodeToward;
        std::function<int(int node)> nodeAway;
        std::function<bool(int node, int& x, int& z)> nodePosition;
        // FUN_000100ec.
        std::function<int()> random;
    };

    // The creature's side of it. The caller copies these in and out of its
    // own creature record.
    struct Agent
    {
        int x = 0, z = 0;           // grid space
        int type = 0;
        int facing = 0;             // +0x42
        int target = 0;             // +0x44, what it is turning toward
        int turnRate = TURN_RATE;   // +0x46
        int speed = 0;              // +0x5e
        int coneHalfWidth = 0x200;  // +0x40
        int nearThreshold = 0x600, middleThreshold = 0x600;   // +0x4a, +0x48
        int wakeRange = 0;          // +0x50
        bool ceilingCrawler = false;

        int state = IDLE;           // +0x6f
        int idleTimer = SPAWN_IDLE_TIMER;   // +0x52
        int attackTimer = 0;        // +0x68
        bool alerted = false;       // +0x6c bit 2
        bool retreating = false;    // +0x6c bit 4

        int waypointX = 0, waypointZ = 0;   // +0x38 / +0x3c
        int vx = 0, vz = 0;         // +0x30 / +0x34

        // Set by the caller each tick.
        int distance = 0;           // +0x60
        int mode = 4;               // +0x73
        int separationY = 0;        // +0x14
        int bearingToPlayer = 0;    // +0x54
        bool lineToPlayer = false;  // FUN_0003231c
        bool zoneVisible = false;   // FUN_00033a1c's first half
        bool playerMakingNoise = false;   // DAT_000b0adc != 0
        bool actFinished = false;   // the act's end (+0xb6 bit 2)

        // Set here, for the caller to act on.
        bool startAct = false;      // FUN_00033cbc: start the attack
        bool resolveHit = false;    // FUN_00033ff8: the attack lands now

        // The queen's own (see above).
        bool queen = false;
        int introPhase = 0;         // her task's +0xc
        bool animationEvent = false;   // set by the caller: +0xb6 bit 1
        int startAnimation = -1;    // set here: FUN_0002f4b0
    };

    inline constexpr int QUEEN_ANIM_WAIT = 8;
    inline constexpr int QUEEN_ANIM_RISE = 9;
    inline constexpr int QUEEN_ANIM_UP = 10;
    inline constexpr int QUEEN_REWAIT = 0x80;
    inline constexpr int QUEEN_HIT_RISE_WAIT = 0x40;
    inline constexpr int QUEEN_SIDESTEP_ATTACK_TIMER = 0x3c;

    // FUN_00032ec4: turn toward `to` by at most the turn rate.
    inline void TurnToward(Agent& a, int to)
    {
        a.target = to & 0xfff;
        if (a.facing == a.target) { return; }
        int d = (a.facing - a.target) & 0xfff;
        if (d >= 0x800) { d -= 0x1000; }
        if (d > 0) { a.facing = (d < a.turnRate) ? a.target : ((a.facing - a.turnRate) & 0xfff); }
        else { a.facing = (-d < a.turnRate) ? a.target : ((a.facing + a.turnRate) & 0xfff); }
    }

    inline void Face(Agent& a, int to) { a.facing = a.target = to & 0xfff; }
    inline void Stop(Agent& a) { a.vx = 0; a.vz = 0; }

    // The distance band the creature task puts every distance into
    // (FUN_00032154 for the player, FUN_00032204 for a waypoint).
    inline int Band(const Agent& a, int distance)
    {
        if (distance < 0x400) { return 0; }
        if (distance < a.nearThreshold) { return 1; }
        if (distance < a.middleThreshold) { return 2; }
        return distance > 0x3fff ? 4 : 3;
    }

    inline int WaypointDistance(const Agent& a)
    {
        return Game::OriginalMath::Distance(a.waypointX - a.x, a.waypointZ - a.z);
    }

    // Is this waypoint worth walking to: within the band the pickers want, on
    // an open cell, and in a clear line.
    inline bool TryWaypoint(Agent& a, World& w, int x, int z, bool strict)
    {
        a.waypointX = x; a.waypointZ = z;
        const int distance = WaypointDistance(a);
        if (!strict)
        {
            const int band = Band(a, distance);
            if (band == 0 || band == 4) { return false; }
            if (!w.cellOpenOrPlayer(x, z)) { return false; }
        }
        else if (!w.cellOpen(x, z)) { return false; }
        return w.lineTo(x, z, distance);
    }

    // FUN_00032fcc: up to four random points 0x400-0x5ff from the PLAYER.
    inline bool PickNearPlayer(Agent& a, World& w, int playerX, int playerZ)
    {
        for (int i = 0; i < PICK_TRIES; ++i)
        {
            const int length = NEAR_PLAYER_MIN + (w.random() & 0x1ff);
            const int angle = w.random() & 0xfff;
            int ox, oz;
            Game::OriginalMath::RotateYaw(length, angle, ox, oz);
            if (TryWaypoint(a, w, playerX + ox, playerZ + oz, false)) { return true; }
        }
        return false;
    }

    // The node pickers' pattern: the node itself, then one random point
    // 0x200-0x3ff around it.
    inline bool TryNode(Agent& a, World& w, int node)
    {
        int nx, nz;
        if (node < 0 || !w.nodePosition(node, nx, nz)) { return false; }
        if (TryWaypoint(a, w, nx, nz, false)) { return true; }
        const int length = NODE_SCATTER_MIN + (w.random() & 0x1ff);
        const int angle = w.random() & 0xfff;
        int ox, oz;
        Game::OriginalMath::RotateYaw(length, angle, ox, oz);
        return TryWaypoint(a, w, nx + ox, nz + oz, false);
    }

    // FUN_000333c8: the next node toward the player, else the creature's own.
    inline bool PickTowardPlayer(Agent& a, World& w)
    {
        const int node = w.nodeAt(a.x, a.z);
        if (node < 0) { return false; }
        if (TryNode(a, w, w.nodeToward(node))) { return true; }
        return TryNode(a, w, node);
    }

    // FUN_000337f4: the next node away from the player.
    inline bool PickAwayFromPlayer(Agent& a, World& w)
    {
        const int node = w.nodeAt(a.x, a.z);
        if (node < 0) { return false; }
        return TryNode(a, w, w.nodeAway(node));
    }

    // FUN_00033ad0: up to four points 0x400 ahead, within 0x200 of straight on.
    inline bool PickSideStep(Agent& a, World& w)
    {
        for (int i = 0; i < PICK_TRIES; ++i)
        {
            const int angle = ((w.random() & 0x3ff) + a.facing + 0x800 - SIDESTEP_SPREAD) & 0xfff;
            int ox, oz;
            Game::OriginalMath::RotateYaw(SIDESTEP, angle, ox, oz);
            if (TryWaypoint(a, w, a.x + ox, a.z + oz, true)) { return true; }
        }
        return false;
    }

    // FUN_00033a70: has the creature noticed the player - inside half its
    // wake range and in view, or inside the whole of it while the player's
    // weapon is still loud.
    inline bool Notices(const Agent& a)
    {
        if (!a.zoneVisible) { return false; }
        if (a.distance < (a.wakeRange >> 1)) { return true; }
        return a.distance < a.wakeRange && a.playerMakingNoise;
    }

    // FUN_00033a1c.
    inline bool SeesInCone(const Agent& a)
    {
        if (!a.zoneVisible) { return false; }
        int d = (a.facing - a.bearingToPlayer) & 0xfff;
        if (d >= 0x800) { d -= 0x1000; }
        if (d < 0) { d = -d; }
        return d <= a.coneHalfWidth;
    }

    // FUN_000322a8: will this step reach the waypoint on every axis it moves.
    inline bool Arrived(const Agent& a)
    {
        const int vx = a.vx < 0 ? -a.vx : a.vx;
        const int vz = a.vz < 0 ? -a.vz : a.vz;
        const int remX = std::abs(a.waypointX - a.x);
        const int remZ = std::abs(a.waypointZ - a.z);
        if (vx == 0 && vz == 0) { return false; }
        if (vx == 0 || vz == 0)
        {
            if (vx != 0) { return vx >= remX; }
        }
        else if (vx < remX) { return false; }
        return remZ <= vz;
    }

    // FUN_00032f78: set off along the facing at the creature's speed.
    inline void SetOff(Agent& a)
    {
        Game::OriginalMath::RotateYaw(a.speed, a.facing + 0x800, a.vx, a.vz);
    }

    inline int Reach(const Agent& a) { return a.ceilingCrawler ? REACH_CEILING : REACH; }

    // FUN_00033cbc's bookkeeping; what the act IS (leap, face hug, swipe) is
    // the caller's.
    inline void Act(Agent& a)
    {
        a.attackTimer = ATTACK_TIMER;
        a.state = ACT;
        a.startAct = true;
    }

    // One tick of the task. The caller has already set the perception fields
    // and counted the timers down.
    // The queen's intro, in IDLE (0x35c95).
    inline void QueenIdle(Agent& a)
    {
        switch (a.introPhase)
        {
        case 3:
            if (a.idleTimer == 0) { a.state = CHASE; }
            return;
        case 2:
            if (a.animationEvent) { a.introPhase = 3; }
            return;
        case 1:
            if (!a.animationEvent) { return; }
            if (a.mode < 2) { a.startAnimation = QUEEN_ANIM_UP; a.introPhase = 2; }
            else { a.startAnimation = QUEEN_ANIM_WAIT; a.introPhase = 0; a.idleTimer = QUEEN_REWAIT; }
            return;
        default:
            if (a.idleTimer == 0 && a.mode < 3) { a.startAnimation = QUEEN_ANIM_RISE; a.introPhase = 1; }
            return;
        }
    }

    // The queen's hit reaction ending (0x3617f).
    inline void QueenRecover(Agent& a, World& w)
    {
        Stop(a);
        if (a.introPhase == 2) { a.introPhase = 3; }
        if (a.introPhase < 2)
        {
            a.startAnimation = QUEEN_ANIM_UP;
            a.introPhase = 2;
            a.state = IDLE;
            a.idleTimer = QUEEN_HIT_RISE_WAIT;
            return;
        }
        if (a.mode < 3 && (a.mode == 0 || a.lineToPlayer))
        {
            Face(a, a.bearingToPlayer);
            Act(a);
            return;
        }
        if (PickSideStep(a, w))
        {
            a.attackTimer = QUEEN_SIDESTEP_ATTACK_TIMER;
            Face(a, Game::OriginalMath::Bearing(a.x - a.waypointX, a.z - a.waypointZ));
            a.state = MOVE;
            return;
        }
        Face(a, a.bearingToPlayer);
        a.state = IDLE;
        a.idleTimer = LOST_LINE_IDLE;
    }

    inline void Step(Agent& a, World& w, int playerX, int playerZ)
    {
        a.startAct = false;
        a.resolveHit = false;
        a.startAnimation = -1;

        switch (a.state)
        {
        case IDLE:
            if (a.queen) { QueenIdle(a); return; }
            Face(a, a.bearingToPlayer);
            if (a.idleTimer == 0) { a.state = CHASE; }
            return;

        case CHASE:
            if (a.retreating)
            {
                if (PickAwayFromPlayer(a, w)) { a.state = MOVE; return; }
                a.retreating = false;
            }
            if (a.alerted)
            {
                if (PickNearPlayer(a, w, playerX, playerZ) || PickTowardPlayer(a, w) || PickSideStep(a, w))
                {
                    a.state = MOVE;
                }
                return;
            }
            if (Notices(a)) { a.alerted = true; return; }
            if (a.queen)
            {
                if (a.zoneVisible && (a.mode == 0 || a.lineToPlayer)) { a.alerted = true; }
                return;
            }
            if (SeesInCone(a))
            {
                if (a.mode == 0 && a.separationY <= CONTACT_VERTICAL && a.lineToPlayer) { a.state = ATTACK; }
                else if (a.lineToPlayer) { a.alerted = true; }
            }
            return;

        case MOVE:
        {
            if (Arrived(a)) { Stop(a); a.state = ATTACK; return; }
            if (!a.retreating && a.attackTimer == 0)
            {
                const bool contact = a.mode == 0 && (a.queen || a.separationY <= CONTACT_VERTICAL);
                const bool inLine = a.mode > 0 && a.mode < 3 && a.lineToPlayer;
                if (contact || inLine)
                {
                    a.attackTimer = ATTACK_TIMER;
                    Stop(a);
                    a.state = ATTACK;
                    return;
                }
            }
            const int toWaypoint = Game::OriginalMath::Bearing(a.x - a.waypointX, a.z - a.waypointZ);
            TurnToward(a, toWaypoint);
            if (a.facing != a.target) { return; }
            if (w.lineTo(a.waypointX, a.waypointZ, WaypointDistance(a))) { SetOff(a); return; }
            Stop(a);
            a.state = IDLE;
            a.idleTimer = LOST_LINE_IDLE;
            return;
        }

        case ATTACK:
            if (a.retreating || a.mode >= 3) { a.state = CHASE; return; }
            if (a.queen)
            {
                if (a.mode == 0 || a.lineToPlayer)
                {
                    TurnToward(a, a.bearingToPlayer);
                    if (a.facing == a.target) { Act(a); }
                    return;
                }
                a.state = CHASE;
                return;
            }
            if (a.separationY <= Reach(a) && a.lineToPlayer)
            {
                TurnToward(a, a.bearingToPlayer);
                if (a.facing == a.target) { Act(a); }
                return;
            }
            a.state = CHASE;
            return;

        case ACT:
            TurnToward(a, a.bearingToPlayer);
            if (!a.actFinished) { return; }
            a.actFinished = false;
            Stop(a);
            a.resolveHit = true;
            if (a.type == 2)
            {
                a.retreating = true;
                Face(a, a.bearingToPlayer);
                a.state = CHASE;
                return;
            }
            if (a.mode < 3)
            {
                if (a.mode == 0 && (a.queen || (a.separationY <= CONTACT_VERTICAL && a.lineToPlayer)))
                {
                    Face(a, a.bearingToPlayer);
                    Act(a);
                    return;
                }
                if (a.lineToPlayer)
                {
                    Face(a, a.bearingToPlayer);
                    Act(a);
                    return;
                }
                if (PickNearPlayer(a, w, playerX, playerZ))
                {
                    a.attackTimer = WANDER_ATTACK_TIMER;
                    Face(a, Game::OriginalMath::Bearing(a.x - a.waypointX, a.z - a.waypointZ));
                    a.state = MOVE;
                    return;
                }
            }
            else if (PickNearPlayer(a, w, playerX, playerZ))
            {
                Face(a, Game::OriginalMath::Bearing(a.x - a.waypointX, a.z - a.waypointZ));
                a.state = MOVE;
                return;
            }
            Face(a, a.bearingToPlayer);
            a.state = IDLE;
            a.idleTimer = AFTER_ACT_IDLE;
            return;

        default:
            return;
        }
    }

    // Which creature types run this task (FUN_0002e638's registrations).
    inline constexpr bool UsesCommonTask(int type)
    {
        return (type >= 2 && type <= 6) || type == 8 || type == 9 || (type >= 0x0b && type <= 0x0e);
    }
}
