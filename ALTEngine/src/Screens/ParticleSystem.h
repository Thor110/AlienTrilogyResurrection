#pragma once

#include "PlayerCamera.h"

#include <cmath>
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
        // Type byte at +0x1e. ALL FIVE ARE NOW TRACED, each to the weapon whose
        // ammo counter its spawn function decrements - which is the only
        // unambiguous link, since the type numbers do not follow the weapon
        // order:
        //     FUN_0002bb74  -> DAT_000b0abe  pistol       type 0
        //     FUN_0002bbc0  -> DAT_000b0ac2  shotgun      type 1
        //     FUN_0002bc18  -> DAT_000b0ac4  pulse rifle  type 2
        //     FUN_0002bcac  -> DAT_000b0aca  flamethrower type 3
        //     FUN_0002c1d4  -> DAT_000b0acc  smartgun     type 4
        //
        // Which weapon owns which counter is settled by the pickups that fill
        // them (pickup 3, "Flamethrower", fills aca; pickup 4, "Smart Gun",
        // fills acc) and by the graphics each weapon slot loads. See
        // PlayerInventoryState::ByIndex. These two were the other way round.
        enum Type : uint8_t
        {
            TYPE_PISTOL = 0,
            TYPE_SHOTGUN = 1,
            TYPE_PULSE = 2,
            TYPE_FLAME = 3,      // FUN_0002bcac's stream, weapon slot 4
            TYPE_SMARTGUN = 4,   // FUN_0002c1d4's aimed rounds, weapon slot 3
            // A guard's, soldier's, synthetic's or handler's round
            // (FUN_0002af8c): 0x80 a tick, 0x40 ticks of life, 1 damage.
            TYPE_ENEMY_ROUND = 5,
            TYPE_FRAGMENT = 7,   // what a projectile shatters INTO
            // Blood a creature sprays when a round hits it (FUN_0002ada4), the
            // type chosen by creature (Game::ProjectileCollision::
            // BloodTypeForCreature). 9 and 10 are acid and burn the player.
            // NAMES: FUN_0002d2bc draws 8 with green and blue taken out of
            // the light - red, a person's blood - 9 green and 10 in the light's
            // own colour. "WHITE" is kept as the name the code already uses.
            TYPE_BLOOD_WHITE = 8,
            TYPE_BLOOD_ACID = 9,
            TYPE_BLOOD_ACID_B = 10,
            // The vents' jets (FUN_0002b0e4, from FUN_000358a4). Each vent type
            // puts out its own: 0x10 steam -> 0x0e, 0x11 flame -> 0x10,
            // 0x13 flame -> 0x11, 0x12 steam -> 0x12.
            TYPE_VENT_STEAM_H = 0x0e,
            TYPE_VENT_FLAME_H = 0x10,
            TYPE_VENT_FLAME_V = 0x11,
            TYPE_VENT_STEAM_V = 0x12,
            TYPE_CASING_B = 0x14,
            TYPE_CASING_A = 0x15,
        };

        // HOW MANY PROJECTILES EACH TRIGGER PULL SPAWNS, and it is not one for
        // three of them:
        //     pulse rifle   3, from FUN_0002bc18's loop, each launched at
        //                   playerSpeed + n * 0x80 - a burst that strings out
        //                   in flight because the later rounds are faster
        //     smartgun      3, FUN_0002c1d4: one aimed round per target it
        //                   finds, or, with none, a staggered-speed burst
        //     flamethrower  FUN_0002c0a0 calling FUN_0002bcac(1), (2), and
        //                   each gets a SHORTER life than the last:
        //                   +0x1c = 0x10 - n. A spray that dies at different
        //                   ranges.
        inline constexpr int ShotsPerPull(int type)
        {
            switch (type)
            {
            case TYPE_PULSE:
            case TYPE_SMARTGUN:    return 3;
            // FUN_0002c0a0: FUN_0002bcac(1), then a loop from 2 while < 3 -
            // two rounds, not three.
            case TYPE_FLAME: return 2;
            default:            return 1;
            }
        }

        // Per-projectile spawn parameters, read straight out of FUN_0002b534's
        // switch.
        //
        // `strength` is the byte at +0x1c, and it is the projectile's LIFETIME
        // IN TICKS. The update loop at 0x2d91c decrements it once per pass and
        // frees the particle the moment it reaches zero. That is what limits a
        // round's range - there is no distance test anywhere, only this counter.
        //
        // It also means the damage rule in FUN_0002a628 - double while
        // strength >= initial/2 - is a RANGE FALLOFF: full damage over the first
        // half of the flight, half over the second.
        //
        //     pistol 20 ticks   shotgun 16   pulse 28   smartgun 28   flamethrower 15 or less
        //
        // `speed` is the short at +0x1a, used both as the damage figure and,
        // shifted left 4, as the distance FUN_0002a448 probes.
        // `lateral` is the sideways muzzle offset, rotated by the view angles.
        //
        // `converge` is a yaw, in 4096 units, that FUN_0002b534 puts into the
        // round's own angles before the view's ([ebp-0x12]: 0x28 for the pistol
        // and shotgun, 0x30 for the pulse rifle, none for the smartgun's type 4). It
        // turns the round back across the muzzle offset, so a round fired from
        // 0x28 to the side crosses the line of sight about 650 units out - the
        // rounds "moving towards" what you aim at.
        struct ProjectileDef
        {
            int strength;
            int speed;
            int lateral;
            int converge = 0;
        };

        inline constexpr ProjectileDef PROJECTILE_PISTOL{ 0x14, 0x10, 0x28, 0x28 };
        inline constexpr ProjectileDef PROJECTILE_SHOTGUN{ 0x10, 0x50, 0x20, 0x28 };
        inline constexpr ProjectileDef PROJECTILE_PULSE{ 0x1c, 0x60, 0x20, 0x30 };

        // The smartgun's, read straight off FUN_0002c1d4, which builds its
        // aimed rounds inline rather than through FUN_0002b534: +0x1c = 0x1c,
        // +0x1a = 0xa0 (the same as FUN_0002b534's type 4 case).
        inline constexpr ProjectileDef PROJECTILE_SMARTGUN{ 0x1c, 0xa0, 0x00 };

        // The flamethrower's, from FUN_0002bcac: +0x1a = 0x50, and +0x1c is
        // 0x10 - n where n is which of the shots it is. The strength here
        // is the FIRST shot's; FlameLife below gives the rest.
        // Its muzzle sits 0x38 to the side (FUN_0002bcac's own offset vector).
        inline constexpr ProjectileDef PROJECTILE_FLAME{ 0x0f, 0x50, 0x38 };

        // The flamethrower builds its own launch rather than going through
        // FUN_0002b534: 0x80 below the eye plus up to 0x1f of jitter, and forward
        // at the player's speed + 0x80 + up to 0x3f - a scatter, not a line.
        inline constexpr int FLAME_MUZZLE_DROP = 0x80;
        inline constexpr int FLAME_HEIGHT_JITTER = 0x1f;
        inline constexpr int FLAME_VELOCITY = 0x80;
        inline constexpr int FLAME_VELOCITY_JITTER = 0x3f;

        inline constexpr int FlameLife(int shotIndex) { return 0x10 - (shotIndex + 1); }

        inline constexpr ProjectileDef DefFor(int type)
        {
            switch (type)
            {
            case TYPE_SHOTGUN:  return PROJECTILE_SHOTGUN;
            case TYPE_PULSE:    return PROJECTILE_PULSE;
            case TYPE_FLAME: return PROJECTILE_FLAME;
            case TYPE_SMARTGUN:    return PROJECTILE_SMARTGUN;
            default:            return PROJECTILE_PISTOL;
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
            case TYPE_PULSE:
            case TYPE_SMARTGUN:
                return 2;
            case TYPE_SHOTGUN:
                return 8;
            default:
                return 3;   // the flamethrower's type 3 lands here
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
        // Spawned at the view position plus (0, 200, 0x280) turned by the
        // pitch and then the view's yaw (+0x40) and roll. The roll is 0x800
        // (DAT_000b0a54), which turns the view's y down the screen, so the 200
        // is 200 BELOW the eye: the case starts low and ahead, at the weapon.
        // Its velocity is (0, -0x30, playerSpeed + 8) turned the same way at
        // a yaw of `random(0..0x7f) + 0x80`: 0x30 UP, flying to the right with
        // a random scatter. This had both the other way up, so cases fell from
        // above the eye.
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
        // The fall and spin, traced: the casings' arm of the particle task
        // (0x2e3df) takes 6 off the vertical speed a tick, and their draw
        // (FUN_0002cf14) turns the first angle 0x40 and the second 0x60 a
        // tick, always the same way. Nothing tests the floor: a case falls on
        // until its 0x20 ticks run out. The pistol's case (0x15) is drawn at
        // half size (0x800), the shotgun's (0x14) at full size; their models
        // are entries 27 and 26 of the pickup model table (0x2480f4 + 4n,
        // read at 0x2b4b9/0x2b4c4).
        inline constexpr int CASING_SPIN_X = 0x40;
        inline constexpr int CASING_SPIN_Y = 0x60;
        inline constexpr int CASING_GRAVITY = 6;
        inline constexpr float CasingDrawScale(int type) { return type == 0x15 ? 0.5f : 1.0f; }

        // Blood falls 4 a tick (the type 8/9/10 arms of the particle task,
        // 0x2e171 and 0x2e1da: `sub word [velocity + 2], 4`).
        inline constexpr int BLOOD_GRAVITY = 4;
        inline constexpr int BLOOD_DROPS = 0x10;
        inline constexpr int BLOOD_DROPS_EXPLOSIVE = 0x40;  // from a 0x0b/0x0c/0x13 source

        inline constexpr bool IsVentParticle(int type)
        {
            return type == TYPE_VENT_STEAM_H || type == TYPE_VENT_FLAME_H
                || type == TYPE_VENT_FLAME_V || type == TYPE_VENT_STEAM_V;
        }

        // A jet touches the player when it is 0 to 0x400 above the player's
        // feet and within 0x100 on both ground axes (FUN_0002a8f8); it then
        // deals its +0x1a (5, or 0xa for the horizontal flame) through
        // FUN_0003e5a8 with cause 5, and is gone.
        inline constexpr int VENT_TOUCH_HEIGHT = 0x400;
        inline constexpr int VENT_TOUCH_HALF = 0x100;
        inline constexpr int VentDamage(int type) { return type == TYPE_VENT_FLAME_H ? 0x0a : 5; }

        inline constexpr bool IsBlood(int type)
        {
            return type == TYPE_BLOOD_WHITE || type == TYPE_BLOOD_ACID || type == TYPE_BLOOD_ACID_B;
        }

        // HOW THE ORIGINAL DRAWS THESE - all traced, from the aggressive
        // re-export. FUN_0002d6ec is the per-particle draw dispatch: it projects
        // the position with FUN_0004884c, keeps the distance, and switches on
        // the type byte to one of several draw routines.
        //
        // They are SCREEN-SPACE PRIMITIVES, not billboards and not meshes. The
        // projection lands in the same 320x240 space the HUD uses (the bounds
        // tested are 0x141 and 0xf1), and each one is inserted into the shared
        // display list with a sort key of distance >> 5 - the same list and the
        // same FUN_00048f64 insert the level geometry uses. That is why they
        // occlude correctly without a depth buffer: they are painter-sorted in
        // with everything else.
        //
        // A BULLET IS A LINE, not a dot (FUN_0002d3e4). It draws from the
        // particle's position to position + a fraction of its velocity, so a
        // round in flight is a short streak pointing the way it is going:
        //     type 0        position .. position + velocity/2
        //     types 2, 4    position .. position + velocity
        // Colour comes from three palette bytes that differ per type.
        //
        // A FRAGMENT IS A 2x2 PIXEL QUAD (FUN_0002d1b4) - four screen points at
        // (x,y), (x+1,y), (x,y+1), (x+1,y+1), shaded by the particle's +0x1a.
        //
        // Casings and the types around them (0x0b-0x0d, 0x13-0x15) go to
        // FUN_0002cf14 instead, before the projection above even happens.
        // IMPACT SOUNDS, from the update loop at 0x2d91c. When a round is
        // stopped by geometry it plays one of three ids through FUN_00040c38,
        // chosen by where it landed rather than by what fired:
        //     0x23  levels 0x0c..0x15
        //     0x24  every other level
        //     0x25  overrides both when the cell attribute underfoot is 8 or 9
        // Attribute 9 is the same one that suppresses footsteps, so 0x25 is
        // almost certainly the wet impact.
        inline constexpr int SFX_IMPACT_ALT_LEVELS = 0x23;
        inline constexpr int SFX_IMPACT_DEFAULT = 0x24;
        inline constexpr int SFX_IMPACT_LIQUID = 0x25;

        inline constexpr int ImpactSound(int levelId, int cellAttribute)
        {
            if (cellAttribute == 8 || cellAttribute == 9) { return SFX_IMPACT_LIQUID; }
            return (levelId > 0x0b && levelId < 0x16) ? SFX_IMPACT_ALT_LEVELS : SFX_IMPACT_DEFAULT;
        }

        inline constexpr int DRAW_NEAR_CUTOFF = 0x200;    // closer than this is not drawn
        inline constexpr int DRAW_FAR_CUTOFF = 0x1801;    // 6145 units, about 12 cells
        inline constexpr int DRAW_SORT_SHIFT = 5;         // display-list key = distance >> 5

        // Fraction of the velocity a tracer streak spans, by type.
        inline constexpr float TracerVelocityFraction(int type)
        {
            return (type == TYPE_PISTOL) ? 0.5f : 1.0f;
        }

        // The extra forward speed the nth shot of a burst gets, from the
        // `n * 0x80 + playerSpeed` the pulse rifle and the smartgun (with no target) both pass.
        inline constexpr int BurstSpeedStep = 0x80;

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
            bool isBlood = false;        // falls at BLOOD_GRAVITY and is gone when it lands
            bool bigBlood = false;       // +0x1d set: from an explosive, drawn four times the size
            int lastTrigger = 0;         // +0x1f, the shot-trigger latch (FUN_000414f0)
            // Steps a flame still owes from its spawn (FUN_0002bcac takes them
            // at once, each through the full collision test, FUN_0002a448).
            int preSteps = 0;

            uint8_t type = TYPE_PISTOL;  // +0x1e
            int strength = 0;            // +0x1d, the value life started at
            int speed = 0;               // +0x1a, damage and probe distance
            int life = 0;                // +0x1c, ticks remaining; zero frees it
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
                p->strength = def.strength;   // the value life started at, for the falloff test
                p->life = def.strength;       // +0x1c, counts down; zero frees the particle
                p->speed = def.speed;

                p->x = eyeX;
                p->y = eyeY - MUZZLE_DROP;
                p->z = eyeZ;
                if (type == TYPE_SMARTGUN) { p->y -= HEAVY_EXTRA_DROP; }
                int forwardSpeed = playerSpeed + MUZZLE_VELOCITY;
                if (type == TYPE_FLAME)
                {
                    p->y = eyeY - FLAME_MUZZLE_DROP + static_cast<float>(rng.Bits(FLAME_HEIGHT_JITTER));
                    forwardSpeed = playerSpeed + FLAME_VELOCITY + rng.Bits(FLAME_VELOCITY_JITTER);
                }

                // Lateral muzzle offset, rotated into the world by the facing.
                const float s = static_cast<float>(PlayerCamera::Sin(viewYaw)) / PlayerCamera::TRIG_ONE;
                const float c = static_cast<float>(PlayerCamera::Cos(viewYaw)) / PlayerCamera::TRIG_ONE;
                p->x += def.lateral * c;
                p->z += def.lateral * s;

                // Forward velocity, pitched. The player's own speed is added, so
                // firing on the move throws the round faster.
                const float forward = static_cast<float>(forwardSpeed);
                const float pitchCos = static_cast<float>(PlayerCamera::Cos(viewPitch)) / PlayerCamera::TRIG_ONE;
                const float pitchSin = static_cast<float>(PlayerCamera::Sin(viewPitch)) / PlayerCamera::TRIG_ONE;

                // The convergence yaw. Increasing the yaw turns the heading
                // toward the side the lateral offset sits on, so it is taken off.
                const int flightYaw = (viewYaw - def.converge) & PlayerCamera::ANGLE_MASK;
                const float fs = static_cast<float>(PlayerCamera::Sin(flightYaw)) / PlayerCamera::TRIG_ONE;
                const float fc = static_cast<float>(PlayerCamera::Cos(flightYaw)) / PlayerCamera::TRIG_ONE;

                p->vx = forward * fs * pitchCos;
                p->vy = forward * pitchSin;
                p->vz = -forward * fc * pitchCos;
                return p;
            }

            // A round launched at a given velocity - the smartgun's aimed shots,
            // which FUN_0002c1d4 builds itself rather than through
            // FUN_0002b534.
            Particle* SpawnAimed(int type, float x, float y, float z, float vx, float vy, float vz)
            {
                Particle* p = Take();
                if (!p) { return nullptr; }
                const ProjectileDef def = DefFor(type);
                p->type = static_cast<uint8_t>(type);
                p->strength = def.strength;
                p->life = def.strength;
                p->speed = def.speed;
                p->x = x; p->y = y; p->z = z;
                p->vx = vx; p->vy = vy; p->vz = vz;
                return p;
            }

            // FUN_0002af8c: a creature's round, already aimed (see
            // CreatureTasks::Gunner). Its +0x1a is the damage, 1.
            Particle* SpawnEnemyRound(float x, float y, float z, float vx, float vy, float vz)
            {
                Particle* p = Take();
                if (!p) { return nullptr; }
                p->type = TYPE_ENEMY_ROUND;
                p->life = p->strength = 0x40;
                p->speed = 1;
                p->x = x; p->y = y; p->z = z;
                p->vx = vx; p->vy = vy; p->vz = vz;
                return p;
            }

            // FUN_0002b37c. Ejected right, with scatter.
            // `viewPitch` is the port's (positive up).
            Particle* SpawnCasing(int casingType, float eyeX, float eyeY, float eyeZ,
                                  int viewYaw, int playerSpeed, int viewPitch = 0)
            {
                Particle* p = Take();
                if (!p) { return nullptr; }

                const float ps = static_cast<float>(PlayerCamera::Sin(viewPitch & PlayerCamera::ANGLE_MASK)) / PlayerCamera::TRIG_ONE;
                const float pc = static_cast<float>(PlayerCamera::Cos(viewPitch & PlayerCamera::ANGLE_MASK)) / PlayerCamera::TRIG_ONE;

                // The view's (0, 200 down, 0x280 ahead), pitched, then along
                // the yaw + 0x40.
                const int spawnYaw = (viewYaw + CASING_YAW_BIAS) & PlayerCamera::ANGLE_MASK;
                const float s = static_cast<float>(PlayerCamera::Sin(spawnYaw)) / PlayerCamera::TRIG_ONE;
                const float c = static_cast<float>(PlayerCamera::Cos(spawnYaw)) / PlayerCamera::TRIG_ONE;
                const float offsetAhead = CASING_OFFSET_FORWARD * pc + CASING_OFFSET_UP * ps;
                const float offsetUp = CASING_OFFSET_FORWARD * ps - CASING_OFFSET_UP * pc;

                p->type = static_cast<uint8_t>(casingType);
                p->life = CASING_LIFETIME;
                p->x = eyeX + offsetAhead * s;
                p->y = eyeY + offsetUp;
                p->z = eyeZ - offsetAhead * c;

                // The view's (0, 0x30 up, speed + 8), pitched, then along the
                // eject yaw.
                const int ejectYaw = (viewYaw + CASING_EJECT_YAW + rng.Bits(CASING_EJECT_SPREAD))
                                   & PlayerCamera::ANGLE_MASK;
                const float es = static_cast<float>(PlayerCamera::Sin(ejectYaw)) / PlayerCamera::TRIG_ONE;
                const float ec = static_cast<float>(PlayerCamera::Cos(ejectYaw)) / PlayerCamera::TRIG_ONE;
                const float forward = static_cast<float>(playerSpeed + CASING_VELOCITY_FORWARD_BIAS);
                const float rise = static_cast<float>(-CASING_VELOCITY_UP);
                const float ahead = forward * pc - rise * ps;

                p->vx = ahead * es;
                p->vy = forward * ps + rise * pc;
                p->vz = -ahead * ec;

                // Random attitude on two axes, third left at zero - traced.
                p->angleX = rng.Bits(PlayerCamera::ANGLE_MASK);
                p->angleY = rng.Bits(PlayerCamera::ANGLE_MASK);
                p->angleZ = 0;

                p->spinX = CASING_SPIN_X;
                p->spinY = CASING_SPIN_Y;
                p->falls = true;
                return p;
            }

            // FUN_0002abe0. The shatter: two pieces for a pistol round, eight
            // for a shotgun pellet, three for anything unrecognised.
            // FUN_0002ada4: sixteen drops from where the round struck, each on a
            // random heading, thrown 0x10-0x1f up and 0x10-0x2f outward, living
            // 0x20-0x2f ticks. Strength 1 - the damage an acid drop does.
            // `fromExplosive`: a grenade or charge hit the creature, and the
            // original sprays 0x40 drops at four times the size (FUN_0002ada4
            // with a 0x0b/0x0c/0x13 source, +0x1d = 1).
            void SpawnBlood(int bloodType, float x, float y, float z, bool fromExplosive = false)
            {
                const int drops = fromExplosive ? BLOOD_DROPS_EXPLOSIVE : BLOOD_DROPS;
                for (int i = 0; i < drops; ++i)
                {
                    Particle* p = Take();
                    if (!p) { return; }
                    p->type = static_cast<uint8_t>(bloodType);
                    p->isBlood = true;
                    p->bigBlood = fromExplosive;
                    p->speed = 1;
                    p->life = rng.Bits(0x0f) + 0x20;
                    p->strength = p->life;
                    p->x = x; p->y = y; p->z = z;

                    const int heading = rng.Bits(PlayerCamera::ANGLE_MASK);
                    const float rise = static_cast<float>(rng.Bits(0x0f) + 0x10);
                    const float out = static_cast<float>(rng.Bits(0x1f) + 0x10);
                    const float s = static_cast<float>(PlayerCamera::Sin(heading)) / PlayerCamera::TRIG_ONE;
                    const float c = static_cast<float>(PlayerCamera::Cos(heading)) / PlayerCamera::TRIG_ONE;
                    p->vx = out * s;
                    p->vy = rise;
                    p->vz = out * c;
                }
            }

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
            // FUN_0002b0e4: one jet from a vent at (x, y, z). Horizontal ones go
            // `speed` along `yaw`; vertical ones straight up, or down for a
            // vent hung from the ceiling.
            Particle* SpawnVent(int type, float x, float y, float z, int speed, int yaw, bool down)
            {
                Particle* p = Take();
                if (!p) { return nullptr; }
                p->type = static_cast<uint8_t>(type);
                p->x = x; p->y = y; p->z = z;
                if (type == TYPE_VENT_FLAME_V || type == TYPE_VENT_STEAM_V)
                {
                    p->vy = static_cast<float>(down ? -speed : speed);
                }
                else
                {
                    p->vx = PlayerCamera::Sin(yaw) / 4096.0f * static_cast<float>(speed);
                    p->vz = -PlayerCamera::Cos(yaw) / 4096.0f * static_cast<float>(speed);
                }
                p->life = 0x20;
                p->strength = p->life;
                p->speed = VentDamage(type);
                return p;
            }

            // The vents' jets, as the particle pass runs them (the arms at
            // 0x2e128, 0x2e0bc, 0x2def1 and 0x2def8): moved by their velocity
            // (FUN_00047524 moves every particle first), aged - the two flames
            // twice as fast - and gone on touching the player. Then:
            //   steam, horizontal  rises 0xa a tick over its last 0x0c ticks;
            //   flame, horizontal  over its last 0x14 ticks drops 2 a tick and
            //                      loses 0xc of its world-Z speed, never going
            //                      below 0 (the original's own, whatever the
            //                      vent's heading);
            //   vertical           held 0x200 under the ceiling (rising) or 0x80
            //                      over the floor (falling), where it turns
            //                      back at 10 a tick and fans out to either
            //                      side by half its speed plus up to 15.
            // `cellHeights(x, z, floorY, ceilingY)` gives the raw floor and
            // ceiling (bytes * 32) of the cell under a point.
            template <typename CellHeightsFn, typename DamageFn>
            void TickVents(CellHeightsFn cellHeights, float playerX, float playerFeetY, float playerZ,
                           DamageFn damagePlayer)
            {
                for (Particle& p : particles)
                {
                    if (!p.alive || !IsVentParticle(p.type)) { continue; }
                    p.x += p.vx; p.y += p.vy; p.z += p.vz;
                    if (--p.life <= 0) { p.alive = false; continue; }
                    if (p.type == TYPE_VENT_FLAME_H || p.type == TYPE_VENT_FLAME_V) { --p.life; }

                    const float above = p.y - playerFeetY;
                    if (above >= 0.0f && above <= VENT_TOUCH_HEIGHT
                        && std::fabs(p.x - playerX) < VENT_TOUCH_HALF
                        && std::fabs(p.z - playerZ) < VENT_TOUCH_HALF)
                    {
                        damagePlayer(p.speed);
                        p.alive = false;
                        continue;
                    }

                    switch (p.type)
                    {
                    case TYPE_VENT_STEAM_H:
                        if (p.life < 0x0c) { p.vy += 0x0a; }
                        break;
                    case TYPE_VENT_FLAME_H:
                        if (p.life < 0x14)
                        {
                            p.vy -= 2;
                            p.vz -= 0x0c;
                            if (p.vz < 0) { p.vz = 0; }
                        }
                        break;
                    default:
                    {
                        float floorY = 0, ceilingY = 0;
                        cellHeights(p.x, p.z, floorY, ceilingY);
                        auto fan = [&]() {
                            const float half = std::trunc(p.vy / 2.0f);
                            const float a = half + static_cast<float>(rng.Next() & 0xf);
                            p.vx = (rng.Next() & 1) ? -a : a;
                            const float b = half + static_cast<float>(rng.Next() & 0xf);
                            p.vz = (rng.Next() & 1) ? -b : b;
                        };
                        if (p.vy > 0 && ceilingY - 0x200 < p.y)
                        {
                            p.y = ceilingY - 0x200;
                            fan();
                            p.vy = -10;
                        }
                        else if (p.vy < 0 && floorY + 0x80 > p.y)
                        {
                            p.y = floorY + 0x80;
                            fan();
                            p.vy = 10;
                        }
                        break;
                    }
                    }
                }
            }

            template <typename BlockedFn>
            void Tick(BlockedFn blocked)
            {
                for (Particle& p : particles)
                {
                    if (!p.alive || IsVentParticle(p.type)) { continue; }

                    // A flame's spawn steps, before it moves on its own. Each
                    // goes through the same test as a flying round, so a crate,
                    // barrel or creature in the first cells takes the hit; a
                    // blocked step leaves the flame burning there.
                    if (p.preSteps > 0)
                    {
                        for (; p.preSteps > 0; --p.preSteps)
                        {
                            const float nx = p.x + p.vx, ny = p.y + p.vy, nz = p.z + p.vz;
                            if (blocked(nx, ny, nz, static_cast<int>(p.type), p.life,
                                        p.x, p.y, p.z, p.lastTrigger) != 0)
                            {
                                p.vx = p.vy = p.vz = 0.0f;
                                p.preSteps = 0;
                                break;
                            }
                            p.x = nx; p.y = ny; p.z = nz;
                        }
                        continue;
                    }

                    // Life first. The original decrements +0x1c at the top of
                    // each pass and frees the particle before it moves it, so a
                    // round on its last tick never travels that tick.
                    if (p.life > 0 && --p.life == 0) { p.alive = false; continue; }

                    // Tumble, and fall if this is something with weight.
                    if (p.spinX != 0 || p.spinY != 0 || p.spinZ != 0)
                    {
                        p.angleX = (p.angleX + p.spinX) & PlayerCamera::ANGLE_MASK;
                        p.angleY = (p.angleY + p.spinY) & PlayerCamera::ANGLE_MASK;
                        p.angleZ = (p.angleZ + p.spinZ) & PlayerCamera::ANGLE_MASK;
                    }
                    if (p.falls) { p.vy -= CASING_GRAVITY; }
                    if (p.isBlood) { p.vy -= BLOOD_GRAVITY; }

                    const bool isProjectile = (p.type == TYPE_PISTOL || p.type == TYPE_SHOTGUN
                                            || p.type == TYPE_PULSE || p.type == TYPE_FLAME
                                            || p.type == TYPE_SMARTGUN || p.type == TYPE_ENEMY_ROUND);

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

                    // The callback gets the candidate position AND the last one
                    // known clear, because a round is stopped by a surface it has
                    // ALREADY crossed - the candidate is inside the wall or under
                    // the floor.
                    //
                    // That is what hid the floor impacts: the effect was spawned
                    // at the candidate, so the sprite sat buried beneath the floor
                    // with nothing above the surface. A wall hit happened to show
                    // because half the quad still stuck out toward the player
                    // (Edward, 2026).
                    // The callback answers CLEAR (0), STOP (1, and a round
                    // shatters) or STOP_QUIETLY (2: a round that hit a creature
                    // sprays blood instead, and leaves no fragments).
                    bool stopped = false;
                    bool shatter = true;
                    float nx = p.x, ny = p.y, nz = p.z;
                    float clearX = p.x, clearY = p.y, clearZ = p.z;
                    for (int step = 0; step < substeps && !stopped; ++step)
                    {
                        clearX = nx; clearY = ny; clearZ = nz;
                        nx += p.vx * inv;
                        ny += p.vy * inv;
                        nz += p.vz * inv;
                        const int verdict = (isProjectile || p.falls || p.isBlood)
                            ? static_cast<int>(blocked(nx, ny, nz, static_cast<int>(p.type), p.life,
                                                       clearX, clearY, clearZ, p.lastTrigger))
                            : 0;
                        if (verdict != 0)
                        {
                            stopped = true;
                            shatter = (verdict != 2);
                            // Leave the round on the clear side, so the shatter
                            // and anything else spawned from its position sits on
                            // the surface rather than through it.
                            nx = clearX; ny = clearY; nz = clearZ;
                        }
                    }

                    // The original never tests a casing against anything: it
                    // falls through the floor, out of sight, until its life
                    // ends (0x2e3df). Gone at the floor or a wall looks the same.
                    if (p.falls && stopped)
                    {
                        p.alive = false;
                        continue;
                    }

                    // Blood is gone the moment it lands (or burns the player);
                    // the callback has already left its splat.
                    if (p.isBlood && stopped) { p.alive = false; continue; }

                    // A flame stops where it is blocked and burns on there for
                    // the rest of its life - the type 0-4 handler at 0x2da89
                    // zeroes a type 3 particle's velocity on a wall or a
                    // creature instead of freeing it, and keeps testing it.
                    if (p.type == TYPE_FLAME && stopped)
                    {
                        p.vx = 0; p.vy = 0; p.vz = 0;
                        p.x = nx; p.y = ny; p.z = nz;
                        continue;
                    }

                    if (isProjectile && stopped)
                    {
                        Particle hit = p;   // copy: Shatter reuses the pool and may move memory
                        p.alive = false;
                        if (shatter) { Shatter(hit, true); }
                        continue;
                    }

                    p.x = nx;
                    p.y = ny;
                    p.z = nz;
                }
            }

        private:
            std::vector<Particle> particles;
            Rng rng;
        };
    }
}
