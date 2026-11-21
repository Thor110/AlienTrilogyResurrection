#pragma once

#include "PlayerCamera.h"
#include "ParticleSystem.h"

#include <cmath>

namespace ALTEngine::Screens
{
    // Items jumping out of a broken crate, from FUN_0003bf64.
    //
    // A dropped pickup is not simply revealed where it was recorded - it is
    // MOVED to the object that dropped it and thrown, and it falls to the floor
    // from there. That is why crate contents seemed to be missing: the level
    // records park them elsewhere entirely, and without the throw they were
    // appearing across the map (Edward, 2026).
    //
    // THE THROW, by the object's facing byte at +0x11:
    //     0   vx  0            vz -(rand(0..0xf)+0x10)
    //     1   vx +(...)        vz -(...)
    //     2   vx +(...)        vz  0
    //     3   vx +(...)        vz +(...)
    //     4   vx  0            vz +(...)
    //     5   vx -(...)        vz +(...)
    //     6   vx -(...)        vz  0
    //     7   vx -(...)        vz -(...)
    // so an item leaves in the direction the crate faces, at 0x10..0x1f. Both
    // components then get a further +/-8 of jitter, so two items out of the same
    // crate do not travel together.
    //
    // Vertical launch is a flat 0x40 (psVar2[5]) and the bounce word
    // psVar2[0x12] is 0x20 (see Step). The spawn height is the floor plus a per-type offset
    // from DAT_000acde8, which is 64 for every type except 20, which gets 96.
    namespace DroppedPickups
    {
        inline constexpr int THROW_BASE = 0x10;     // minimum horizontal speed
        inline constexpr int THROW_RANDOM = 0x0f;   // added on top
        inline constexpr int THROW_JITTER = 8;      // +/- on each axis afterwards
        inline constexpr int THROW_UP = 0x40;       // vertical launch
        inline constexpr int DROP_LIFETIME = 0x20;

        // Spawn height above the floor, DAT_000acde8 indexed by pickup type.
        inline constexpr int DROP_HEIGHT_DEFAULT = 64;
        inline constexpr int DROP_HEIGHT_TYPE_20 = 96;

        inline int DropHeight(int pickupType)
        {
            return (pickupType == 20) ? DROP_HEIGHT_TYPE_20 : DROP_HEIGHT_DEFAULT;
        }

        // How large each type is drawn: the uniform scale on the diagonal of
        // its 3x3 matrix in DAT_000acbb8 (12.12 fixed point, 0x10 bytes a type).
        // 28 types; the table runs on into other data after that.
        inline float DrawScale(int pickupType)
        {
            static constexpr int SCALE[28] = {
                4096, 4096, 4096, 4096, 5120, 1024, 7168, 7168, 7168, 7168,
                7168, 7168, 7168, 7168, 7168, 7168, 7168, 3072, 11264, 11264,
                10240, 8192, 7168, 7168, 7168, 7168, 7168, 7168,
            };
            if (pickupType < 0 || pickupType >= 28) { return 1.75f; }
            return static_cast<float>(SCALE[pickupType]) / 4096.0f;
        }

        // Type 0x18 is rewritten to 0x15 as it drops - the uncapped health type.
        // Whatever 0x18 means elsewhere, a crate never yields one.
        inline int RemapDroppedType(int pickupType)
        {
            return (pickupType == 0x18) ? 0x15 : pickupType;
        }

        struct Flight
        {
            float vx = 0, vy = 0, vz = 0;
            bool falling = false;
        };

        // Builds the launch for one item. `facing` is the crate's rotation byte.
        inline Flight Launch(int facing, Particles::Rng& rng)
        {
            Flight flight;
            const auto speed = [&] {
                return static_cast<float>(rng.Bits(THROW_RANDOM) + THROW_BASE);
            };

            switch (facing & 7)
            {
            case 0: flight.vx = 0;        flight.vz = -speed(); break;
            case 1: flight.vx = speed();  flight.vz = -speed(); break;
            case 2: flight.vx = speed();  flight.vz = 0;        break;
            case 3: flight.vx = speed();  flight.vz = speed();  break;
            case 4: flight.vx = 0;        flight.vz = speed();  break;
            case 5: flight.vx = -speed(); flight.vz = speed();  break;
            case 6: flight.vx = -speed(); flight.vz = 0;        break;
            default:flight.vx = -speed(); flight.vz = -speed(); break;
            }

            // The jitter both components get afterwards.
            flight.vx += static_cast<float>(rng.Bits(THROW_RANDOM) - THROW_JITTER);
            flight.vz += static_cast<float>(rng.Bits(THROW_RANDOM) - THROW_JITTER);
            flight.vy = static_cast<float>(THROW_UP);
            flight.falling = true;
            return flight;
        }

        // One pass of an item in the air, FUN_0003c258 (run on alternate
        // ticks). Grid-space position; `bounce` is the word at +0x24, which
        // the throw sets to DROP_LIFETIME (0x20).
        //
        //   x then z: moved by the speed, unless the cell moved into has no
        //   faces, holds an object (+0xc > 0x13), is a wall (attribute
        //   0x14-0x2c), is more than 0x100 above or below the floor it left,
        //   or is attribute 6 with its ceiling under the item's y + 0x80 - then
        //   it is put back against that cell's edge (the last unit of the cell
        //   before it going forward, the first of the one after going back).
        //   z is tested from the x it ended on.
        //   y: the speed is added first. Below its rest height (floor + the
        //   type's DropHeight) it lands: the bounce word drops by 8 and is the
        //   new upward speed, or 0 once it is under 4, and the sideways speeds
        //   are zeroed. In the air the speed drops by 4, except that 4 becomes
        //   -2. At rest it follows the floor.
        struct Body
        {
            int x = 0, y = 0, z = 0;
            int vx = 0, vy = 0, vz = 0;
            int bounce = DROP_LIFETIME;
        };

        // Returns true while it is still moving.
        template <typename CellFn, typename FloorFn, typename CeilingFn>
        inline bool Step(Body& b, int pickupType, CellFn&& cellAt, FloorFn&& floorAt, CeilingFn&& ceilingAt)
        {
            const int startFloor = floorAt(b.x, b.z);
            auto blocked = [&](int x, int z) {
                const auto* cell = cellAt(x, z);
                if (!cell) { return true; }
                if ((cell->faceRun0 | cell->faceRun1 | cell->faceRun2 | cell->faceRun3) == 0) { return true; }
                if (cell->occupancy > 0x13) { return true; }
                if (cell->attribute > 0x13 && cell->attribute < 0x2d) { return true; }
                if (cell->attribute < 0x14)
                {
                    const int rise = floorAt(x, z) - startFloor;
                    if (rise > 0x100 || rise < -0x100) { return true; }
                    if (cell->attribute == 6 && ceilingAt(x, z) < b.y + 0x80) { return true; }
                }
                return false;
            };
            auto clampTo = [](int moved, int speed) {
                const int cellStart = moved & ~0x1ff;
                return speed < 0 ? cellStart + 0x200 : cellStart - 1;
            };
            if (b.vx != 0)
            {
                const int nx = b.x + b.vx;
                b.x = blocked(nx, b.z) ? clampTo(nx, b.vx) : nx;
            }
            if (b.vz != 0)
            {
                const int nz = b.z + b.vz;
                b.z = blocked(b.x, nz) ? clampTo(nz, b.vz) : nz;
            }

            const int rest = floorAt(b.x, b.z) + DropHeight(pickupType);
            if (b.vy == 0) { b.y = rest; }
            else
            {
                b.y += b.vy;
                if (b.y < rest)
                {
                    b.y = rest;
                    b.bounce -= 8;
                    b.vy = b.bounce < 4 ? 0 : b.bounce;
                    b.vx = 0;
                    b.vz = 0;
                }
                else { b.vy = b.vy == 4 ? -2 : b.vy - 4; }
            }
            return b.vx != 0 || b.vy != 0 || b.vz != 0;
        }
    }
}
