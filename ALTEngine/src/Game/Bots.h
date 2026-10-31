#pragma once

#include "../Formats/LevelLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

namespace ALTEngine::Game::Bots
{
    // MULTIPLAYER BOTS - the port's own, a variant of the Synthetic's task.
    //
    // The original has no bots: its multiplayer maps hold four Synthetic
    // records as the player starts (FUN_0002e638, case 0xd). These play like
    // the Synthetic does in the campaign - follow the path network toward a
    // player (the flow field, FUN_00033114), and fire once there is a clear
    // line (FUN_0003231c) - but they hunt every player, not just one, and
    // carry a player's pulse rifle so they can be fought on even terms.
    //
    // The host runs them; each takes a player slot, so to everyone else a bot
    // is a player like any other (its state, shots, hits and frags go through
    // Net::Session as a player's do).
    //
    // Everything is in WORLD coordinates (the gameplay screen's), on the
    // player's own tick (PlayerCamera::TICK_HZ); angles are the original's
    // 4096 to the turn, x += sin, z -= cos.

    inline constexpr int MIN_BOTS = 3;
    inline constexpr int MAX_BOTS = 7;
    inline constexpr int MIN_SKILL = 1;
    inline constexpr int MAX_SKILL = 5;
    inline constexpr int HEALTH = 100;
    // Ticks dead before coming back (about three seconds).
    inline constexpr int RESPAWN_TICKS = 210;
    // The player's walk speed (PlayerCamera WALK, 0x78).
    inline constexpr int PLAYER_WALK = 0x78;
    // How close a node has to be to count as reached.
    inline constexpr float NODE_REACHED = 0x100;
    // Beyond this a target is not looked for (the Synthetic's line test gives
    // up at 0x2000, CreatureSight::MAX_DISTANCE; a bot sees twice as far).
    inline constexpr float SIGHT_RANGE = 0x4000;
    // Closer than this a bot stops walking in and strafes.
    inline constexpr float KEEP_DISTANCE = 0x600;

    struct Skill
    {
        int reactionTicks = 0;   // a target in view this long before the first shot
        int aimSpread = 0;       // aim error either way, 4096 to the turn
        int fireInterval = 0;    // ticks between rounds
        int turnRate = 0;        // per tick
        int walkSpeed = 0;       // per tick, world units
    };

    // 1 (easiest) to 5. 3 is about a player walking and firing a pulse rifle
    // in bursts; 5 reacts in a fifth of a second and rarely misses.
    inline Skill SkillFor(int level)
    {
        const int s = std::clamp(level, MIN_SKILL, MAX_SKILL) - 1;   // 0..4
        static constexpr Skill TABLE[5] = {
            { 70, 0x90, 40, 0x30, 0x50 },
            { 50, 0x60, 28, 0x48, 0x60 },
            { 35, 0x40, 18, 0x60, 0x70 },
            { 22, 0x28, 12, 0x80, 0x78 },
            { 14, 0x14, 8, 0xa0, 0x78 },
        };
        return TABLE[s];
    }

    struct Bot
    {
        int slot = -1;
        float x = 0, z = 0, feetY = 0;
        int yaw = 0;
        int health = HEALTH;
        bool dead = false;
        int respawnTicks = 0;
        bool moving = false;
        int targetSlot = -1;     // who it is after, -1 none
        int seenTicks = 0;       // how long that target has been in view
        int cooldown = 0;        // ticks to the next round
        int goalNode = -1;       // the node it is heading for
        int stuckTicks = 0;
        int strafe = 1;          // which way it sidesteps when close
        int strafeTicks = 0;
        int lastAttacker = -1;   // who hit it last, for the frag
        uint32_t rng = 0x9e3779b9u;

        int Random(int mask)
        {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            return static_cast<int>(rng & static_cast<uint32_t>(mask));
        }
    };

    // Someone a bot can go after: every other player and bot.
    struct Target
    {
        int slot = -1;
        float x = 0, feetY = 0, z = 0;
        bool alive = true;
    };

    struct World
    {
        const std::vector<Formats::PathNode>* nodes = nullptr;
        // The path node of the cell at a point (cell byte +6), or -1.
        std::function<int(float x, float z)> nodeAt;
        // A node's own point.
        std::function<bool(int node, float& x, float& z)> nodePosition;
        // Whether a round from one point would reach the other.
        std::function<bool(float x0, float y0, float z0, float x1, float y1, float z1)> clearShot;
        // The player's mover: where a step from (x, z) ends up.
        std::function<void(float& x, float& z, float feetY, float stepX, float stepZ)> move;
        std::function<float(float x, float z)> floor;
    };

    struct Output
    {
        bool fire = false;
        int aimYaw = 0;          // with the spread in
        float aimX = 0, aimY = 0, aimZ = 0;   // the point aimed at (target's body)
    };

    inline int AngleTo(float fromX, float fromZ, float toX, float toZ)
    {
        const double a = std::atan2(static_cast<double>(toX - fromX), -static_cast<double>(toZ - fromZ));
        return static_cast<int>(std::lround(a * 4096.0 / 6.283185307179586)) & 0xfff;
    }

    // The signed difference b - a, -2048..2047.
    inline int AngleDelta(int a, int b) { return ((b - a + 0x800) & 0xfff) - 0x800; }

    inline int TurnToward(int yaw, int want, int rate)
    {
        const int d = AngleDelta(yaw, want);
        if (std::abs(d) <= rate) { return want & 0xfff; }
        return (yaw + (d > 0 ? rate : -rate)) & 0xfff;
    }

    // The next node on the way from `from` to `to`, by a breadth-first walk
    // of the path network (the flow field's own fill, FUN_000281cc, from the
    // goal), or -1.
    inline int NextNode(const std::vector<Formats::PathNode>& nodes, int from, int to)
    {
        const int n = static_cast<int>(nodes.size());
        if (from < 0 || to < 0 || from >= n || to >= n) { return -1; }
        if (from == to) { return to; }
        std::vector<int> parent(static_cast<size_t>(n), -2);
        std::vector<int> queue{ to };
        parent[static_cast<size_t>(to)] = -1;
        for (size_t head = 0; head < queue.size(); ++head)
        {
            const auto& node = nodes[static_cast<size_t>(queue[head])];
            for (uint8_t next : { node.nodeA, node.nodeB, node.nodeC, node.nodeD })
            {
                if (next >= n || parent[next] != -2) { continue; }
                parent[next] = queue[head];
                if (next == from) { return queue[head]; }
                queue.push_back(next);
            }
        }
        return -1;
    }

    // One tick. Picks the nearest player it can see (or failing that the
    // nearest at all), walks the path network toward them, turns to face
    // them once in view and fires after its reaction time.
    inline Output Tick(Bot& bot, const Skill& skill, const World& world, const std::vector<Target>& targets)
    {
        Output out;
        bot.moving = false;
        if (bot.dead) { return out; }
        if (bot.cooldown > 0) { bot.cooldown--; }

        // The target: nearest seen; else nearest.
        const Target* best = nullptr;
        bool bestSeen = false;
        float bestDist = 0;
        for (const Target& t : targets)
        {
            if (!t.alive || t.slot == bot.slot) { continue; }
            const float dx = t.x - bot.x, dz = t.z - bot.z;
            const float dist = std::sqrt(dx * dx + dz * dz);
            const bool seen = dist <= SIGHT_RANGE && world.clearShot
                && world.clearShot(bot.x, bot.feetY + 0x240, bot.z, t.x, t.feetY + 0x280, t.z);
            if (!best || (seen && !bestSeen) || (seen == bestSeen && dist < bestDist))
            {
                best = &t; bestSeen = seen; bestDist = dist;
            }
        }
        if (!best)
        {
            bot.targetSlot = -1;
            bot.seenTicks = 0;
            return out;
        }
        if (best->slot != bot.targetSlot) { bot.targetSlot = best->slot; bot.seenTicks = 0; }
        bot.seenTicks = bestSeen ? bot.seenTicks + 1 : 0;

        // Where to walk: straight at a target in view (strafing once close),
        // otherwise along the path network.
        float goalX = best->x, goalZ = best->z;
        bool walk = true;
        if (!bestSeen && world.nodes && world.nodeAt && world.nodePosition)
        {
            const int here = world.nodeAt(bot.x, bot.z);
            const int there = world.nodeAt(best->x, best->z);
            const int next = NextNode(*world.nodes, here, there);
            float nx = 0, nz = 0;
            if (next >= 0 && world.nodePosition(next, nx, nz))
            {
                // Already on the node's own spot: aim for the one after it.
                if (std::hypot(nx - bot.x, nz - bot.z) < NODE_REACHED && next != there)
                {
                    const int after = NextNode(*world.nodes, next, there);
                    float ax = 0, az = 0;
                    if (after >= 0 && world.nodePosition(after, ax, az)) { nx = ax; nz = az; }
                }
                goalX = nx; goalZ = nz;
            }
        }
        int moveYaw = AngleTo(bot.x, bot.z, goalX, goalZ);
        if (bestSeen && bestDist < KEEP_DISTANCE)
        {
            // Close in: sidestep, turning the other way now and then.
            if (--bot.strafeTicks <= 0) { bot.strafe = -bot.strafe; bot.strafeTicks = 40 + bot.Random(63); }
            moveYaw = (moveYaw + bot.strafe * 0x400) & 0xfff;
        }

        // Facing: the target once seen, the way it walks otherwise.
        const int faceYaw = bestSeen ? AngleTo(bot.x, bot.z, best->x, best->z) : moveYaw;
        bot.yaw = TurnToward(bot.yaw, faceYaw, skill.turnRate);

        if (walk && world.move)
        {
            const float sx = static_cast<float>(skill.walkSpeed) * static_cast<float>(std::sin(moveYaw * 6.283185307179586 / 4096.0));
            const float sz = -static_cast<float>(skill.walkSpeed) * static_cast<float>(std::cos(moveYaw * 6.283185307179586 / 4096.0));
            const float oldX = bot.x, oldZ = bot.z;
            world.move(bot.x, bot.z, bot.feetY, sx, sz);
            if (world.floor) { bot.feetY = world.floor(bot.x, bot.z); }
            const float moved = std::hypot(bot.x - oldX, bot.z - oldZ);
            bot.moving = moved > 1.0f;
            // Wedged on something: try the other strafe for a while.
            if (moved < static_cast<float>(skill.walkSpeed) / 4.0f)
            {
                if (++bot.stuckTicks > 20) { bot.strafe = -bot.strafe; bot.strafeTicks = 30; bot.stuckTicks = 0; }
            }
            else { bot.stuckTicks = 0; }
        }

        // Fire: in view, faced near enough, after the reaction time.
        if (bestSeen && bot.seenTicks >= skill.reactionTicks && bot.cooldown == 0
            && std::abs(AngleDelta(bot.yaw, faceYaw)) < 0x100)
        {
            out.fire = true;
            const int spread = skill.aimSpread > 0 ? bot.Random(0x3ff) % (2 * skill.aimSpread + 1) - skill.aimSpread : 0;
            out.aimYaw = (faceYaw + spread) & 0xfff;
            out.aimX = best->x;
            out.aimY = best->feetY + 0x280;
            out.aimZ = best->z;
            bot.cooldown = skill.fireInterval + bot.Random(7);
        }
        return out;
    }

    // Damage to a bot: a quarter of the round's, as a player takes it (the
    // original's type 8). Returns true when it dies of it.
    inline bool TakeHit(Bot& bot, int damage, int attacker)
    {
        if (bot.dead) { return false; }
        bot.health -= std::max(1, damage / 4);
        bot.lastAttacker = attacker;
        if (bot.health > 0) { return false; }
        bot.health = 0;
        bot.dead = true;
        bot.respawnTicks = RESPAWN_TICKS;
        bot.targetSlot = -1;
        bot.seenTicks = 0;
        return true;
    }
}
