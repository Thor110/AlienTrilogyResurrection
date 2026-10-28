#pragma once

#include "../Formats/OriginalTables.h"

#include <cstdint>
#include <cstdlib>

namespace ALTEngine::Game::OriginalMath
{
    // The original's integer maths, for the places where its exact rounding
    // decides an outcome (a probe landing one side of a cell edge or the
    // other). Each is checked against the routine itself, run in an emulator.

    inline int Sin(int angle) { return Formats::OriginalTables::SINE[angle & 0xfff]; }
    inline int Cos(int angle) { return Formats::OriginalTables::SINE[(angle + 0x400) & 0xfff]; }

    // FUN_0004de04: square root, scaled by 1024. Exact from a 256-entry table
    // (isqrt(n << 20)), and above that 2 * root(n >> 2) - which is why it runs
    // a little low for large n.
    inline int64_t Sqrt1024(int64_t n)
    {
        if (n < 0) { return 0; }
        if (n > 0xff) { return 2 * Sqrt1024(n >> 2); }
        int64_t r = 0;
        while ((r + 1) * (r + 1) <= (n << 20)) { ++r; }
        return r;
    }

    // FUN_000320f8: the distance the creature task works with (+0x60).
    inline int Distance(int dx, int dz)
    {
        dx = std::abs(dx); dz = std::abs(dz);
        if (dx >= 0x4000 || dz >= 0x4000) { return 0x4000; }
        return static_cast<int>(Sqrt1024(static_cast<int64_t>(dz) * dz + static_cast<int64_t>(dx) * dx) >> 10);
    }

    // FUN_00026b64(a, b) with dx = a.x - b.x, dz = a.z - b.z: the bearing, 4096
    // to the turn, by an arcsine table. Called as (creature, player) it is the
    // way the creature must face to look at the player.
    inline int Bearing(int dx, int dz)
    {
        int ax = std::abs(dx), az = std::abs(dz);
        const int biggest = ax > az ? ax : az;
        if (biggest > 0x4000) { ax >>= 2; az >>= 2; }
        else if (biggest < 0x400) { ax <<= 2; az <<= 2; }
        const int r = static_cast<int>(Sqrt1024(static_cast<int64_t>(ax) * ax + static_cast<int64_t>(az) * az) >> 10);
        if (r < 2) { return 0; }
        const int u = (ax << 10) / r;
        int v = (az << 10) / r;
        int angle;
        if (u < 0x2d4) { angle = Formats::OriginalTables::ARCSINE[u]; }
        else
        {
            if (v > 0x2d3) { v = 0x2d3; }
            angle = 0x400 - Formats::OriginalTables::ARCSINE[v];
        }
        if (dz < 0) { angle = 0x800 - angle; }
        if (dx < 0) { angle = 0x1000 - angle; }
        return (0x1000 - angle) & 0xfff;
    }

    // FUN_00026a20 with a pure yaw: (0, 0, length) turned by `angle`.
    inline void RotateYaw(int length, int angle, int& x, int& z)
    {
        x = (-length * Sin(angle)) >> 12;
        z = (length * Cos(angle)) >> 12;
    }
}
