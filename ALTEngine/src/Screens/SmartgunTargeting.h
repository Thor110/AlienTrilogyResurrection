#pragma once

#include "PlayerCamera.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ALTEngine::Screens::SmartgunTargeting
{
    // THE SMARTGUN AIMS ITSELF - FUN_0002c1d4 (weapon slot 3).
    //
    // FUN_0002c12c(0x80) first lists the creatures it may lock on to:
    //   - type 0x0e or below, and in state 5 or below (not hit, not dying);
    //   - in a cell whose zone bits meet the player's view mask (DAT_000b0cf0);
    //   - no more than 0x1c00 away (the creature's own +0x60 distance);
    //   - within 0x80 either side of the player's heading, measured against
    //     the creature's bearing from the player (+0x54).
    //
    // FUN_0002c1d4 then fires up to three rounds per pull. Each takes the
    // nearest target not yet used and is aimed straight at it:
    //   - it starts 0x100 below the eye and 0x190 ahead along the view;
    //   - its pitch is the angle from there to the creature's Y plus half its
    //     box's half height, and its yaw is the player's heading less the
    //     target's bearing offset - so it flies at the creature, wherever the
    //     view is pointing within the cone.
    // Shots left over when the list runs out go straight ahead through
    // FUN_0002b534 (type 4), at playerSpeed + n * 0x80.
    //
    // Each round costs one from DAT_000b0acc.
    inline constexpr int CONE = 0x80;
    inline constexpr int RANGE = 0x1c00;
    inline constexpr int MAX_TARGET_TYPE = 0x0e;
    inline constexpr int MAX_TARGET_STATE = 5;
    inline constexpr int SHOTS = 3;
    inline constexpr float LAUNCH_DROP = 0x100;
    inline constexpr float LAUNCH_AHEAD = 0x190;
    inline constexpr int SPEED = 0x100;   // + the player's own, as FUN_0002b534

    struct Candidate
    {
        int index;      // into the caller's creature list
        int distance;
        int offset;     // signed heading - bearing, 4096 units
    };

    // The player's bearing to a point, in the port's angle convention (the
    // one PlayerCamera::Sin/Cos and SpawnProjectile use: forward is
    // (sin, -cos)).
    inline int BearingFrom(float fromX, float fromZ, float toX, float toZ)
    {
        return static_cast<int>(std::lround(
            std::atan2(toX - fromX, -(toZ - fromZ))
            * (PlayerCamera::ANGLE_UNITS / 6.28318530718))) & PlayerCamera::ANGLE_MASK;
    }

    // Signed difference of two 4096-unit angles, -2048..2047 - the original's
    // shl 4 / sar 4 sign extension of a 12-bit value.
    inline int SignedDelta(int a, int b)
    {
        int d = (a - b) & PlayerCamera::ANGLE_MASK;
        if (d >= PlayerCamera::ANGLE_UNITS / 2) { d -= PlayerCamera::ANGLE_UNITS; }
        return d;
    }

    struct Creature
    {
        float x, z;
        int type;
        int state;
        int distance;
        bool eligible;   // active, alive and in a zone the player can see
    };

    // The target list, nearest first.
    inline std::vector<Candidate> FindTargets(const std::vector<Creature>& creatures,
                                              float playerX, float playerZ, int playerYaw)
    {
        std::vector<Candidate> out;
        for (size_t i = 0; i < creatures.size(); ++i)
        {
            const Creature& c = creatures[i];
            if (!c.eligible || c.type > MAX_TARGET_TYPE || c.state > MAX_TARGET_STATE) { continue; }
            if (c.distance > RANGE) { continue; }
            const int offset = SignedDelta(playerYaw, BearingFrom(playerX, playerZ, c.x, c.z));
            if (offset > CONE || offset < -CONE) { continue; }
            out.push_back({ static_cast<int>(i), c.distance, offset });
        }
        std::stable_sort(out.begin(), out.end(),
                         [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
        return out;
    }

    // Velocity for a round from (x, y, z) at the given point, at `speed`.
    struct Vec { float x, y, z; };
    inline Vec AimVelocity(float x, float y, float z, float tx, float ty, float tz, float speed)
    {
        const float dx = tx - x, dy = ty - y, dz = tz - z;
        const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 1.0f) { return { 0.0f, 0.0f, 0.0f }; }
        return { dx / len * speed, dy / len * speed, dz / len * speed };
    }
}
