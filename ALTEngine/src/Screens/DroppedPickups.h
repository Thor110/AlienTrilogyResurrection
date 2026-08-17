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
    // Vertical launch is a flat 0x40 (psVar2[5]) and the lifetime-ish field
    // psVar2[0x12] is 0x20. The spawn height is the floor plus a per-type offset
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

        // Gravity is OURS - the original's fall is handled by the shared entity
        // update, which has not been read. Marked.
        inline constexpr float GRAVITY = 6.0f;
    }
}
