#pragma once

#include "PlayerCamera.h"

#include <cstdint>
#include <vector>

namespace ALTEngine::Screens
{
    // Bullets, shell casings and impact fragments.
    //
    // All three are the SAME pooled object in the original, distinguished only
    // by a type byte at +0x1e. One free list (DAT_00247734) and one active list
    // (DAT_00247730), taken from and returned to by every effect in the game.
    //
    //   FUN_0002b534  spawns a projectile        (called by every weapon)
    //   FUN_0002b37c  spawns a shell casing      (called after a shot lands)
    //   FUN_0002abe0  shatters one into fragments on impact
    //   FUN_0002aa7c  unlinks a dead one back to the free list
    //   FUN_0002a448  the collision test the projectile uses per step
    //
    // Angles are the same 4096-per-turn units the camera uses, and the trig
    // table is shared - see PlayerCamera.h.
    namespace Particles
    {
        // Type byte at +0x1e. These are the values the code branches on.
        enum Type : uint8_t
        {
            TYPE_PISTOL = 0,
            TYPE_SHOTGUN = 1,
            TYPE_FLAME = 2,
            TYPE_UNKNOWN_3 = 3,
            TYPE_HEAVY = 4,      // spawns 0xa0 lower and much faster; pulse or smartgun
            TYPE_FRAGMENT = 7,   // what a projectile shatters INTO
            TYPE_CASING_B = 0x14,
            TYPE_CASING_A = 0x15,
        };

        // Per-projectile spawn parameters, read straight out of FUN_0002b534's
        // switch. `strength` is the byte it writes to +0x1c (and mirrors into
        // +0x1d); `speed` is the short at +0x1a; `lateral` is the sideways
        // muzzle offset it rotates by the view angles before adding.
        struct ProjectileDef
        {
            int strength;
            int speed;
            int lateral;
        };

        inline constexpr ProjectileDef PROJECTILE_PISTOL{ 0x14, 0x10, 0x28 };
        inline constexpr ProjectileDef PROJECTILE_SHOTGUN{ 0x10, 0x50, 0x20 };
        inline constexpr ProjectileDef PROJECTILE_FLAME{ 0x1c, 0x60, 0x20 };
        inline constexpr ProjectileDef PROJECTILE_HEAVY{ 0x1c, 0xa0, 0x00 };

        inline constexpr ProjectileDef DefFor(int type)
        {
            switch (type)
            {
            case TYPE_SHOTGUN: return PROJECTILE_SHOTGUN;
            case TYPE_FLAME:   return PROJECTILE_FLAME;
            case TYPE_HEAVY:   return PROJECTILE_HEAVY;
            default:           return PROJECTILE_PISTOL;
            }
        }

        // Muzzle height: FUN_0002b534 starts every projectile at the VIEW
        // position with Y - 0x60, then adds the rotated lateral offset. Type 4
        // drops a further 0xa0.
        inline constexpr int MUZZLE_DROP = 0x60;
        inline constexpr int HEAVY_EXTRA_DROP = 0xa0;

        // Forward speed added to the player's own: FUN_0002b534 builds the
        // velocity's forward component as `playerSpeed + 0x100`, so a shot fired
        // while running is genuinely faster than one fired standing still.
        inline constexpr int MUZZLE_VELOCITY = 0x100;

        // HOW MANY PIECES A PROJECTILE SHATTERS INTO, from FUN_0002abe0's own
        // switch on the source type. This is the behaviour Edward described from
        // playing it - a pistol round breaking into two on a wall - and the code
        // agrees: case 0 gives exactly 2.
        inline constexpr int FragmentCount(int sourceType)
        {
            switch (sourceType)
            {
            case TYPE_PISTOL:
            case TYPE_FLAME:
            case TYPE_HEAVY:
                return 2;
            case TYPE_SHOTGUN:
                return 8;
            default:
                return 3;
            }
        }

        inline constexpr int FRAGMENT_STRENGTH = 0x10;

        // FUN_0002abe0's spread, chosen by its second argument. What that
        // argument distinguishes is NOT traced - a wall hit versus a body hit is
        // the obvious guess but nothing confirms it, so both are kept and named
        // after their values rather than after a meaning.
        inline constexpr int FRAGMENT_SPREAD_WIDE = 0x40;   // arg == 0
        inline constexpr int FRAGMENT_SPREAD_TIGHT = 0x10;  // arg != 0

        // Shell casing, from FUN_0002b37c.
        //
        // Spawned at the view position plus (0, 200, 0x280) rotated by the view
        // angles with the yaw biased +0x40, which puts it up and forward of the
        // eye and slightly right - the muzzle. Its velocity is (0, -0x30,
        // playerSpeed + 8) thrown at a yaw of `random(0..0x7f) + 0x80`, so it
        // always flies to the RIGHT with a random scatter, which is what an
        // ejection port does.
        inline constexpr int CASING_OFFSET_UP = 200;
        inline constexpr int CASING_OFFSET_FORWARD = 0x280;
        inline constexpr int CASING_YAW_BIAS = 0x40;
        inline constexpr int CASING_VELOCITY_UP = -0x30;
        inline constexpr int CASING_VELOCITY_FORWARD_BIAS = 8;
        inline constexpr int CASING_EJECT_YAW = 0x80;
        inline constexpr int CASING_EJECT_SPREAD = 0x7f;
        inline constexpr int CASING_LIFETIME = 0x20;

        // Longest distance a particle may move between collision tests.
        inline constexpr float MAX_SUBSTEP = 128.0f;

        // CASING TUMBLE. FUN_0002b37c gives a fresh casing two RANDOM
        // orientation angles and leaves the third at zero:
        //     +0x10 = random & 0xfff
        //     +0x12 = random & 0xfff
        //     +0x14 = 0
        // Those are the sprite instance's own euler angles, in the same
        // 4096-per-turn units as everything else, so a casing leaves the port
        // already at a random attitude rather than upright. That part is traced.
        //
        // THE SPIN RATE AND THE FALL ARE NOT. The loop that advances a live
        // particle's angles and pulls it down has not been read out yet - the
        // spawners are all traced, the updater is not. The three values below
        // are therefore GUESSES, chosen to look like a small brass case
        // tumbling out of a pistol at 30 ticks/second, and they are the first
        // thing to replace once that loop is found.
        inline constexpr int CASING_SPIN_X = 0x60;   // GUESS - angle units per tick
        inline constexpr int CASING_SPIN_Y = 0x38;   // GUESS
        inline constexpr int CASING_GRAVITY = 12;    // GUESS - world units/tick^2

        struct Particle
        {
            // Position and velocity, world units. The original keeps these as
            // shorts behind two pointers at +0x00 and +0x04; kept as plain
            // members here since nothing else shares the storage.
            float x = 0, y = 0, z = 0;
            float vx = 0, vy = 0, vz = 0;

            // Orientation and its rate of change. The original keeps the three
            // angles at +0x10, +0x12 and +0x14; the rates are ours (see
            // CASING_SPIN_X).
            int angleX = 0, angleY = 0, angleZ = 0;
            int spinX = 0, spinY = 0, spinZ = 0;
            bool falls = false;          // gravity applies - casings only

            uint8_t type = TYPE_PISTOL;  // +0x1e
            int strength = 0;            // +0x1c
            int speed = 0;               // +0x1a
            int life = 0;                // +0x20 for casings; fragments get FRAGMENT_STRENGTH
            bool alive = false;
        };

        // A small xorshift, used wherever the original calls FUN_000100ec.
        //
        // NOT the original's generator. Its sequence is not reproduced, only its
        // RANGE - every use here masks the result the same way the original does
        // (& 0x1f, & 0x0f, & 0xfff), so the spread of values matches even though
        // the exact stream does not. That matters only if something ever needs
        // to be deterministic against a recording.
        struct Rng
        {
            uint32_t state = 0x2545f491u;
            uint32_t Next()
            {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                return state;
            }
            int Bits(uint32_t mask) { return static_cast<int>(Next() & mask); }
        };

        // The pool. One vector standing in for the original's two intrusive
        // lists - the distinction only mattered because it was allocating from a
        // fixed arena.
        class Pool
        {
        public:
            // 128 is the original's own pool size - FUN_0002a9cc builds the
            // free list with a loop bounded at 0x80, each object 44 bytes with
            // its position and velocity in separate parallel arrays. Every
            // effect in the game draws from this one pool, which is why a busy
            // firefight can starve the casings.
            static constexpr size_t ORIGINAL_CAPACITY = 0x80;

            explicit Pool(size_t capacity = ORIGINAL_CAPACITY) { particles.resize(capacity); }

            const std::vector<Particle>& All() const { return particles; }
            size_t LiveCount() const
            {
                size_t n = 0;
                for (const Particle& p : particles) { if (p.alive) { n++; } }
                return n;
            }

            void Clear() { for (Particle& p : particles) { p.alive = false; } }

            Particle* Take()
            {
                for (Particle& p : particles)
                {
                    if (!p.alive) { p = Particle{}; p.alive = true; return &p; }
                }
                return nullptr; // pool exhausted - the original just drops the effect too
            }

            // FUN_0002b534. `viewYaw`/`viewPitch` are the camera's, in 4096
            // units; `playerSpeed` is the original's DAT_000b0a62 high half.
            Particle* SpawnProjectile(int type, float eyeX, float eyeY, float eyeZ,
                                      int viewYaw, int viewPitch, int playerSpeed)
            {
                Particle* p = Take();
                if (!p) { return nullptr; }

                const ProjectileDef def = DefFor(type);
                p->type = static_cast<uint8_t>(type);
                p->strength = def.strength;
                p->speed = def.speed;

                p->x = eyeX;
                p->y = eyeY - MUZZLE_DROP;
                p->z = eyeZ;
                if (type == TYPE_HEAVY) { p->y -= HEAVY_EXTRA_DROP; }

                // Lateral muzzle offset, rotated into the world by the facing.
                const float s = static_cast<float>(PlayerCamera::Sin(viewYaw)) / PlayerCamera::TRIG_ONE;
                const float c = static_cast<float>(PlayerCamera::Cos(viewYaw)) / PlayerCamera::TRIG_ONE;
                p->x += def.lateral * c;
                p->z += def.lateral * s;

                // Forward velocity, pitched. The player's own speed is added, so
                // firing on the move throws the round faster.
                const float forward = static_cast<float>(playerSpeed + MUZZLE_VELOCITY);
                const float pitchCos = static_cast<float>(PlayerCamera::Cos(viewPitch)) / PlayerCamera::TRIG_ONE;
                const float pitchSin = static_cast<float>(PlayerCamera::Sin(viewPitch)) / PlayerCamera::TRIG_ONE;

                p->vx = forward * s * pitchCos;
                p->vy = forward * pitchSin;
                p->vz = -forward * c * pitchCos;
                return p;
            }

            // FUN_0002b37c. Ejected right, with scatter.
            Particle* SpawnCasing(int casingType, float eyeX, float eyeY, float eyeZ,
                                  int viewYaw, int playerSpeed)
            {
                Particle* p = Take();
                if (!p) { return nullptr; }

                const int spawnYaw = (viewYaw + CASING_YAW_BIAS) & PlayerCamera::ANGLE_MASK;
                const float s = static_cast<float>(PlayerCamera::Sin(spawnYaw)) / PlayerCamera::TRIG_ONE;
                const float c = static_cast<float>(PlayerCamera::Cos(spawnYaw)) / PlayerCamera::TRIG_ONE;

                p->type = static_cast<uint8_t>(casingType);
                p->life = CASING_LIFETIME;
                p->x = eyeX + CASING_OFFSET_FORWARD * s;
                p->y = eyeY + CASING_OFFSET_UP;
                p->z = eyeZ - CASING_OFFSET_FORWARD * c;

                const int ejectYaw = (viewYaw + CASING_EJECT_YAW + rng.Bits(CASING_EJECT_SPREAD))
                                   & PlayerCamera::ANGLE_MASK;
                const float es = static_cast<float>(PlayerCamera::Sin(ejectYaw)) / PlayerCamera::TRIG_ONE;
                const float ec = static_cast<float>(PlayerCamera::Cos(ejectYaw)) / PlayerCamera::TRIG_ONE;
                const float forward = static_cast<float>(playerSpeed + CASING_VELOCITY_FORWARD_BIAS);

                p->vx = forward * es;
                p->vy = static_cast<float>(CASING_VELOCITY_UP);
                p->vz = -forward * ec;

                // Random attitude on two axes, third left at zero - traced.
                p->angleX = rng.Bits(PlayerCamera::ANGLE_MASK);
                p->angleY = rng.Bits(PlayerCamera::ANGLE_MASK);
                p->angleZ = 0;

                // Tumble and fall - see the note on CASING_SPIN_X. The sign of
                // the spin is randomised so consecutive casings do not all
                // rotate identically.
                const int sx = (rng.Bits(1) != 0) ? CASING_SPIN_X : -CASING_SPIN_X;
                const int sy = (rng.Bits(1) != 0) ? CASING_SPIN_Y : -CASING_SPIN_Y;
                p->spinX = sx;
                p->spinY = sy;
                p->falls = true;
                return p;
            }

            // FUN_0002abe0. The shatter: two pieces for a pistol round, eight
            // for a shotgun pellet, three for anything unrecognised.
            void Shatter(const Particle& source, bool wideSpread)
            {
                const int count = FragmentCount(source.type);
                const int spread = wideSpread ? FRAGMENT_SPREAD_WIDE : FRAGMENT_SPREAD_TIGHT;

                for (int i = 0; i < count; ++i)
                {
                    Particle* p = Take();
                    if (!p) { return; } // pool exhausted mid-burst, as the original's while loop also allows

                    p->type = TYPE_FRAGMENT;
                    p->strength = FRAGMENT_STRENGTH;
                    p->life = FRAGMENT_STRENGTH;

                    // Fragments start exactly where the source died - the
                    // original copies all three coordinates verbatim.
                    p->x = source.x;
                    p->y = source.y;
                    p->z = source.z;

                    const int yaw = rng.Bits(PlayerCamera::ANGLE_MASK);
                    const float s = static_cast<float>(PlayerCamera::Sin(yaw)) / PlayerCamera::TRIG_ONE;
                    const float c = static_cast<float>(PlayerCamera::Cos(yaw)) / PlayerCamera::TRIG_ONE;

                    const float lateral = static_cast<float>(rng.Bits(0x1f) + 0x10);
                    const float rise = static_cast<float>(rng.Bits(0x0f) + 0x10);
                    const float away = static_cast<float>(spread + rng.Bits(0x1f));

                    p->vx = lateral * s;
                    p->vy = rise;
                    p->vz = -away * c;
                }
            }

            // One tick of movement.
            //
            // RECONSTRUCTED, NOT TRANSCRIBED - and the only part of this file
            // that is. FUN_0002b534's spawn does two collision probes via
            // FUN_0002a448 before the projectile is even linked in, so the
            // per-tick advance clearly works the same way, but the loop that
            // actually runs it has not been read out yet. `blocked` is the
            // caller's own collision test.
            //
            // Everything a fragment or casing does after it is created - gravity,
            // bounce, how it dies - is likewise not traced. They currently just
            // travel and expire.
            template <typename BlockedFn>
            void Tick(BlockedFn blocked)
            {
                for (Particle& p : particles)
                {
                    if (!p.alive) { continue; }

                    // Tumble, and fall if this is something with weight.
                    if (p.spinX != 0 || p.spinY != 0 || p.spinZ != 0)
                    {
                        p.angleX = (p.angleX + p.spinX) & PlayerCamera::ANGLE_MASK;
                        p.angleY = (p.angleY + p.spinY) & PlayerCamera::ANGLE_MASK;
                        p.angleZ = (p.angleZ + p.spinZ) & PlayerCamera::ANGLE_MASK;
                    }
                    if (p.falls) { p.vy -= CASING_GRAVITY; }

                    const bool isProjectile = (p.type == TYPE_PISTOL || p.type == TYPE_SHOTGUN
                                            || p.type == TYPE_FLAME || p.type == TYPE_HEAVY);

                    // Substep. A pistol round covers 376 units a tick and a
                    // crate is about 256 across, so testing only the endpoint
                    // lets a fast round step over a small target. Splitting the
                    // move into pieces no longer than a target is wide costs
                    // nothing at these speeds and removes the whole class of
                    // problem for walls too.
                    const float stepLength = std::sqrt(p.vx * p.vx + p.vy * p.vy + p.vz * p.vz);
                    const int substeps = (stepLength > MAX_SUBSTEP)
                                       ? static_cast<int>(stepLength / MAX_SUBSTEP) + 1 : 1;
                    const float inv = 1.0f / static_cast<float>(substeps);

                    bool stopped = false;
                    float nx = p.x, ny = p.y, nz = p.z;
                    for (int step = 0; step < substeps && !stopped; ++step)
                    {
                        nx += p.vx * inv;
                        ny += p.vy * inv;
                        nz += p.vz * inv;
                        if ((isProjectile || p.falls) && blocked(nx, ny, nz, static_cast<int>(p.type)))
                        {
                            stopped = true;
                        }
                    }

                    // A casing that reaches the floor stops there and rests out
                    // its remaining life. NOT TRACED - whether the original
                    // bounces, slides or simply removes them is in the same
                    // unread update loop.
                    if (p.falls && stopped)
                    {
                        p.vx = 0; p.vy = 0; p.vz = 0;
                        p.spinX = 0; p.spinY = 0; p.spinZ = 0;
                        p.falls = false;
                        if (p.life > 0 && --p.life == 0) { p.alive = false; }
                        continue;
                    }

                    if (isProjectile && stopped)
                    {
                        Particle hit = p;   // copy: Shatter reuses the pool and may move memory
                        p.alive = false;
                        Shatter(hit, true);
                        continue;
                    }

                    p.x = nx;
                    p.y = ny;
                    p.z = nz;

                    if (p.life > 0 && --p.life == 0) { p.alive = false; }
                }
            }

        private:
            std::vector<Particle> particles;
            Rng rng;
        };
    }
}
