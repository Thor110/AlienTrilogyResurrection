#pragma once

#include "../Formats/LevelLoader.h"

#include <functional>

namespace ALTEngine::Game::PlayerMover
{
    // THE PLAYER'S MOVE, one axis at a time - FUN_0003d85c (x) and
    // FUN_0003dc44 (z), called from FUN_0003dff0, x first and z at the new x.
    // All in grid space (the original's own coordinates).
    //
    // A move of v along an axis probes a point 200 ahead of the target
    // (x + v +/- 200) at three positions across it: the player's own line, 200
    // to one side and 200 to the other. For each:
    //   - a WALL stops the move: the cell's +7 bit 0x80, which the level load
    //     sets for attribute 1 and 0x14-0x18 (0x282ed) and doors set on their
    //     cells, or a creature standing in it (+0xc). The noclip cheat
    //     (DAT_000a4d64 bit 0) ignores both. A stopped move goes to the slide
    //     (FUN_0003d804 / FUN_0003dbec): the player is put 200 short of the
    //     blocked cell's near edge, if that spot is itself clear, or stays.
    //   - for an ordinary cell (attribute below 0x14, or 0x19-0x2c) the
    //     HEIGHTS: a ceiling lower than the player's Y + 0x300 stops the move,
    //     and so does a floor more than 0x100 above the player.
    //   - a diagonal wall cell (0x19-0x1c) clamps the probe to its own side of
    //     the diagonal (FUN_0002800c / FUN_000280dc).
    // The result is the target, unless the clamped probe holds the player
    // back from it. Last, a DROP of more than 0x100 where the player arrives
    // puts them 0xc9 (going up the axis) or 0x138 (going down) into that cell,
    // clear of the lip, so they step off cleanly rather than straddle it.
    //
    // OURS: the original reads cells outside the map unchecked; here they are
    // walls.
    struct World
    {
        // The cell at a grid-space point, or null off the map.
        std::function<const Formats::CollisionNode*(int x, int z)> cell;
        // A creature stands in this cell (+0xc).
        std::function<bool(int x, int z)> occupied;
        // FUN_00027e28 / FUN_00027fb0: the floor and the ceiling at a point.
        std::function<int(int x, int z)> floor;
        std::function<int(int x, int z)> ceiling;
        bool noclip = false;
    };

    inline constexpr int PROBE_LEAD = 200;     // 0xc8
    inline constexpr int HEADROOM = 0x300;
    inline constexpr int STEP = 0x100;
    inline constexpr int LIP_UP = 0xc9;        // into a dropping cell, going up the axis
    inline constexpr int LIP_DOWN = 0x138;     // going down it

    // The +7 wall bit as the load sets it.
    inline bool WallAttribute(int attribute)
    {
        return attribute == 1 || (attribute >= 0x14 && attribute <= 0x18);
    }

    inline bool HeightChecked(int attribute)
    {
        return attribute < 0x14 || (attribute >= 0x19 && attribute <= 0x2c);
    }

    inline bool Blocked(const World& w, int x, int z)
    {
        if (w.noclip) { return false; }
        const auto* c = w.cell(x, z);
        if (!c) { return true; }
        return WallAttribute(c->attribute) || (w.occupied && w.occupied(x, z));
    }

    // FUN_0002800c: a diagonal cell keeps `x` on its own side.
    inline int ClampDiagonalX(const World& w, int x, int z)
    {
        const auto* c = w.cell(x, z);
        if (!c) { return x; }
        const int fx = x & 0x1ff, fz = z & 0x1ff, base = x & ~0x1ff;
        switch (c->attribute)
        {
        case 0x19: { const int lim = 0x200 - fz - 1; return fx <= lim ? base + lim + 1 : x; }
        case 0x1a: { const int lim = fz; return fx <= lim ? base + lim + 1 : x; }
        case 0x1b: { const int lim = fz; return fx >= lim ? base + lim - 1 : x; }
        case 0x1c: { const int lim = 0x200 - fz - 1; return fx >= lim ? base + lim - 1 : x; }
        default: return x;
        }
    }

    // FUN_000280dc: the same for `z`.
    inline int ClampDiagonalZ(const World& w, int x, int z)
    {
        const auto* c = w.cell(x, z);
        if (!c) { return z; }
        const int fx = x & 0x1ff, fz = z & 0x1ff, base = z & ~0x1ff;
        switch (c->attribute)
        {
        case 0x19: { const int lim = 0x200 - fx - 1; return fz <= lim ? base + lim + 1 : z; }
        case 0x1a: { const int lim = fx; return fz <= lim ? base + lim + 1 : z; }
        case 0x1b: { const int lim = fx; return fz >= lim ? base + lim - 1 : z; }
        case 0x1c: { const int lim = 0x200 - fx - 1; return fz >= lim ? base + lim - 1 : z; }
        default: return z;
        }
    }

    namespace Detail
    {
        // One axis. `along`/`across` are the moving and fixed coordinates;
        // `at(a, c)` maps them back to (x, z).
        template <typename At, typename Clamp>
        int MoveAxis(const World& w, int along, int across, int y, int v, At at, Clamp clamp)
        {
            const bool negative = v < 0;
            int probe = along + v + (negative ? -PROBE_LEAD : PROBE_LEAD);

            // The slide: 200 short of the blocked cell, if that is clear.
            auto slide = [&]() {
                const int stop = (probe & ~0x1ff) + (negative ? 0x200 + PROBE_LEAD : -(PROBE_LEAD + 1));
                int sx = 0, sz = 0;
                at(stop, across, sx, sz);
                return Blocked(w, sx, sz) ? along : stop;
            };

            const int sides[3] = { across, across - PROBE_LEAD, across + PROBE_LEAD };
            for (int side : sides)
            {
                int px = 0, pz = 0;
                at(probe, side, px, pz);
                if (Blocked(w, px, pz)) { return slide(); }
                const auto* c = w.cell(px, pz);
                if (c && HeightChecked(c->attribute))
                {
                    if (w.ceiling(px, pz) < y + HEADROOM) { return slide(); }
                    const int f = w.floor(px, pz);
                    if (f > y && f - y > STEP) { return slide(); }
                }
                probe = clamp(probe, side);
            }

            const int target = along + v;
            int result = negative ? (probe >= target ? probe : target)
                                  : (probe <= target ? probe : target);

            int rx = 0, rz = 0;
            at(result, across, rx, rz);
            const int f = w.floor(rx, rz);
            if (f < y && y - f > STEP)
            {
                const int base = result & ~0x1ff;
                if (negative) { if (result > base + LIP_DOWN) { result = base + LIP_DOWN; } }
                else if (result < base + LIP_UP) { result = base + LIP_UP; }
            }
            return result;
        }
    }

    // FUN_0003d85c: the new x for a move of vx from (x, z), standing at y.
    inline int MoveX(const World& w, int x, int z, int y, int vx)
    {
        if (vx == 0) { return x; }
        return Detail::MoveAxis(w, x, z, y, vx,
            [](int a, int c, int& ox, int& oz) { ox = a; oz = c; },
            [&](int a, int c) { return ClampDiagonalX(w, a, c); });
    }

    // FUN_0003dc44: the new z for a move of vz from (x, z).
    inline int MoveZ(const World& w, int x, int z, int y, int vz)
    {
        if (vz == 0) { return z; }
        return Detail::MoveAxis(w, z, x, y, vz,
            [](int a, int c, int& ox, int& oz) { ox = c; oz = a; },
            [&](int a, int c) { return ClampDiagonalZ(w, c, a); });
    }

    // THE PUSHES FUN_0003dff0 adds to the move.
    //
    // A slippery ramp (attributes 0x35-0x38, the same slopes as 0x31-0x34)
    // pushes the player downhill: by their own speed (at least 0x20), or 0x80
    // standing still; running (DAT_000b0ab4 bit 2), by their speed less 0x20.
    // Downhill is -z, +x, -x, +z for 0x35-0x38, in that order.
    inline bool RampPush(int attribute, int speed, bool running, int& outX, int& outZ)
    {
        if (attribute < 0x35 || attribute > 0x38) { return false; }
        const int s = speed == 0 ? 0x80 : (speed < 0x20 ? 0x20 : (running ? speed - 0x20 : speed));
        static constexpr int DX[4] = { 0, 1, -1, 0 };
        static constexpr int DZ[4] = { -1, 0, 0, 1 };
        outX = DX[attribute - 0x35] * s;
        outZ = DZ[attribute - 0x35] * s;
        return true;
    }

    // Knockback (DAT_000b0ade, its direction DAT_000b0ae0): a strength that
    // pushes the player along a heading and fades by 8 a tick. A creature's
    // directed hit that takes health sets 0x80 along the attacker's facing
    // (FUN_0003e5d4); the shotgun's shot sets 0x40 and the grenade's 0x20,
    // straight back from the view (FUN_0002bbc0, 0x2c52c).
    inline constexpr int KNOCKBACK_HIT = 0x80;
    inline constexpr int KNOCKBACK_SHOTGUN = 0x40;
    inline constexpr int KNOCKBACK_GRENADE = 0x20;
    inline constexpr int KNOCKBACK_FADE = 8;
}
