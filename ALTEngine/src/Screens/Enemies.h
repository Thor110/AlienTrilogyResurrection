#pragma once

#include "../Formats/LevelLoader.h"
#include "../Formats/SpriteAnimator.h"
#include "DamageSystem.h"
#include "EnemyBehaviour.h"
#include "PlayerCamera.h"
#include "../Game/OriginalMath.h"
#include "CreatureAI.h"
#include "CreatureTasks.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ALTEngine::Screens
{
    // Enemies.
    //
    // WHAT THE LEVEL GIVES US, per monster record (20 bytes):
    //   type, x, y, rotation (8-direction), health, drop, difficulty, speed
    //
    // Health and speed are PER INSTANCE, and difficulty is per instance too -
    // measured on L111: type 2 appears at health 1 and 2 with speeds 50, 80 and
    // 100 across difficulties 0/1/2, and type 6 at health 30 with speeds 100 and
    // 120. So the same enemy is tuned individually per placement rather than
    // scaled globally, which is why object health has no difficulty field: only
    // monsters need one.
    //
    // ENTITY STATE is the same byte the damage model already uses - see
    // Damage::EntityState. FUN_000310ac sets 6 on a hit reaction and
    // FUN_00030b04 sets 7 on death, and anything above 5 is immune, which is the
    // hit-invulnerability window that stops a burst deleting everything at once.
    //
    // WHAT IS NOT DONE HERE: movement, pathing, attacks. This is spawning,
    // per-instance state and the damage hookup - the parts the level data and
    // the already-traced damage model fully determine. The AI itself is a
    // separate trace.
    namespace Enemies
    {
        // ENEMY HEALTH, and difficulty DOES scale it - just not per record.
        //
        // FUN_0002e52c computes the entity's health when it spawns:
        //
        //     shift  = (record type == 7) ? 6 : 4
        //     base   = record health byte
        //     if the global difficulty is 1 and base != 0   base += 1
        //     if the global difficulty is 2 and base != 0   base += 2
        //     health = base << shift
        //     if health != 0 and health < 0x10   health = 0x10
        //
        // So the record carries a SMALL base - L111's facehuggers are 1 and its
        // type 6 is 30 - and the real figure is that shifted left four places.
        // Difficulty adds to the base BEFORE the shift, so on Hard a facehugger
        // goes from 16 to 48: three times the health, not a third more.
        //
        // Type 7 shifts by six instead of four, which is sixteen times the
        // health of the same base elsewhere. Whatever type 7 is, it is meant to
        // be very hard to kill.
        //
        // The difficulty itself is GLOBAL - FUN_00013078 returns it, the same
        // getter the particle draw dispatch uses. There is no per-monster
        // difficulty field; that was a misreading of the trigger threshold.
        inline constexpr int HEALTH_SHIFT_DEFAULT = 4;
        inline constexpr int HEALTH_SHIFT_TYPE_7 = 6;
        inline constexpr int HEALTH_SHIFT_TYPE = 7;
        inline constexpr int HEALTH_MINIMUM = 0x10;

        inline int SpawnHealth(int recordType, int recordHealth, int difficulty)
        {
            const int shift = (recordType == HEALTH_SHIFT_TYPE) ? HEALTH_SHIFT_TYPE_7
                                                                : HEALTH_SHIFT_DEFAULT;
            int base = recordHealth;
            if (base != 0)
            {
                if (difficulty == 1) { base += 1; }
                else if (difficulty == 2) { base += 2; }
            }
            int health = base << shift;
            if (health != 0 && health < HEALTH_MINIMUM) { health = HEALTH_MINIMUM; }
            return health;
        }


        // How fast a creature turns toward the player, and how its record's speed
        // byte converts to world units. BOTH GUESSES - the original turns through
        // FUN_00032f78 and stores its velocities in fields fed by the entity
        // template, neither of which has been read.
        inline constexpr int TURN_RATE = 0x60;

        // How close to facing the player a creature must be before it advances.
        // 0x200 of 4096 is about 45 degrees either side. GUESS.
        inline constexpr int ALIGN_BEFORE_MOVING = 0x200;
        inline constexpr float SPEED_SCALE = 0.5f;


        // WHEN A MONSTER ENTERS PLAY.
        //
        // Not a difficulty setting - a trigger count. FUN_0002f288 increments
        // the record's byte 7 and activates the monster when it reaches byte 8,
        // the same gate FUN_0003bf64 uses for pickups. Threshold 0 means it is
        // already in play when the level starts.
        //
        // This was implemented as difficulty gating, which would have removed
        // three of L111's monsters on Easy for no reason. See the note on
        // Monster::triggerThreshold.
        inline bool ActiveAtStart(const ALTEngine::Formats::Monster& record)
        {
            return record.triggerThreshold == 0;
        }

        // A monster waiting to be let out is parked off the playable map - on
        // L111 all six sit at x = 9, in a row down the edge. Five are named by a
        // crate; the sixth waits on a script.
        inline constexpr int PARKED_X = 9;

        // The subtypes that spring out of a crate, from FUN_0002f288: it only
        // does the jump-out setup for entity subtype 2 or 3, starting animation
        // 10 or 12 respectively, setting the frame duration to 0x60 and the
        // state to 5.
        //
        // Subtype 2 is the one L111 is full of - 24 of its 28 monsters - and it
        // is what the crates hold. The facehugger.
        inline constexpr int SPRING_SUBTYPE_A = 2;
        inline constexpr int SPRING_SUBTYPE_B = 3;
        inline constexpr int SPRING_ANIM_A = 10;
        inline constexpr int SPRING_ANIM_B = 12;
        inline constexpr int SPRING_FRAME_DURATION = 0x60;
        inline constexpr int SPRING_STATE = 5;

        struct Enemy
        {
            int type = 0;
            int monsterIndex = -1;      // its record in level.monsters

            float x = 0, y = 0, z = 0;
            int facing = 0;             // 4096-per-turn, from the 8-direction byte

            int health = 0;
            int baseHealth = 0;         // the record's own byte, before the shift
            int speed = 0;
            int state = Damage::STATE_NORMAL;
            int hitCooldown = 0;
            uint8_t drop = 0xFF;        // what it leaves behind
            uint8_t triggerThreshold = 0;
            uint8_t triggerCount = 0;
            bool springing = false;     // playing the jump-out

            // Perception, refreshed every tick (FUN_00032154).
            float separationX = 0, separationY = 0, separationZ = 0;
            int distance = 0;
            int mode = 4;               // the distance band, 0 closest
            int ticks = 0;
            bool spawnDeathEffect = false;

            // Damage to apply to the player this tick, 0 for none.
            int pendingDamage = 0;
            int attackCooldown = 0;

            // This tick's intended movement, before collision. The caller applies
            // it, because only it can probe the level.
            float stepX = 0, stepZ = 0;

            // THE VELOCITY FLAG the motion tracker tests. Not a latch after all:
            // set when the creature moves, and cleared when it ATTACKS
            // (FUN_00033ff8 zeroes both axes) or dies. That alternation is the
            // per-contact flicker.
            bool velocityActive = false;

            // Mid-leap. While set the creature is out of the ordinary state
            // machine, as the original's handler swap does.
            bool pouncing = false;
            int pounceTicks = 0;

            // THE LEAP, the attack every melee creature makes when it is not
            // face-hugging (FUN_00033cbc). Vertical velocity is the entity's
            // +0x32 and the mover (FUN_00031f3c) applies it with gravity; the
            // horizontal velocity is set once at take-off and held. This is what
            // makes a hugger hittable with level fire: mid-leap it rises right
            // through the line rounds fly along.
            bool leaping = false;
            bool engaged = false;       // in the original's attack state (4)

            // Set by the caller each tick, since only it can see the level:
            // FUN_0003231c's clear straight line to the player, and
            // FUN_00033a1c's zone test. Every attack needs both.
            bool lineOfFire = true;
            bool zoneVisible = true;

            // The common creature task's own state (CreatureAI.h), for the
            // types that run it. Only used when the caller supplies a world.
            CreatureAI::Agent ai;
            CreatureTasks::Egg::Egg egg;   // type 1's own task
            CreatureTasks::Vent::Vent vent;   // types 0x10-0x13
            CreatureTasks::Colonist::Colonist colonist;   // type 10
            int facingOctant = 0;       // +0x6d, the record's rotation
            int heldMonster = 0xff;     // a colonist's burster (+0x5e)
            bool cried = false;         // a colonist called out this tick
            int dropPickup = -1;        // a pickup to throw out this tick
            // A command chain to run as this creature dies: the queen's record
            // byte 6 (her state 8, FUN_00040e2c). -1 for none.
            int deathChain = -1;
            bool ventDown = false;      // a vertical vent hung from the ceiling
            // What a vent did this tick, for the caller: put out a jet, start
            // its sound, burst (an explosion), or switch off for good.
            bool ventEmit = false;
            bool ventSound = false;
            bool ventBurst = false;
            bool ventSwitchedOff = false;
            bool recoveredFromHit = false;
            // A monster this creature lets out this tick (an egg's hugger),
            // for the caller to release; -1 for none.
            int releaseMonster = -1;
            int actTicks = 0;           // a non-leap attack still playing
            float vy = 0;
            float leapX = 0, leapZ = 0;

            // Set by the caller when the overlay has slid back off. The creature
            // dies then, not on a timer - FUN_000307c0 writes state 2 at the
            // bottom of the fall and LAB_00030918 is waiting for exactly that.
            bool overlayFinished = false;

            // WHICH ANIMATION IS PLAYING, and where in it. The original starts an
            // animation through FUN_0002f4b0 whenever behaviour changes and the
            // shared VM advances it; this tracks the same thing so the sprite has
            // something to draw.
            int animation = -1;
            int animationFrame = 0;
            int animationTimer = 0;
            bool animationChanged = false;
            // The playing animation's frame count, set by the caller from the
            // sprite set (0 when it doesn't know), and whether it has played
            // through - the stand-in for the script's end bit (+0xb6 bit 2).
            // A creature that holds its animations (the egg) stays on the last
            // frame instead of looping.
            int animationFrames = 0;
            bool animationEnded = false;
            bool holdAnimation = false;
            float lastX = 0, lastZ = 0;

            // The two per-creature distance thresholds the mode banding uses.
            // GUESSES - the original reads them from entity fields +0x48 and
            // +0x46, populated from the entity template, which has not been read.
            //
            // MELEE CREATURES GET MUCH SHORTER ONES. The attack table lets
            // subtypes 2 and 3 hit at mode 2 as well as 0 and 1, and with these
            // set generously that meant a face hugger clawing at the player from
            // 4096 units away (Edward, 2026: "they seem to shoot me or damage me
            // from a distance"). The table is traced and right; the thresholds
            // feeding it were mine and far too wide.
            //
            // Set from the subtype in Build(), so a hugger has to be nearly
            // touching before even its mode-2 nibble lands.
            int nearThreshold = 0x800;
            int middleThreshold = 0x1000;

            // The attack cone half-width, the original's +0x40: the record's
            // bytes 10/11, clamped (Build). 0x200 is what most records hold.
            int coneHalfWidth = 0x200;

            bool active = false;        // released and in play
            bool alive = true;

            ALTEngine::Formats::SpriteAnim::Animator animator;
        };

        // The record's rotation is an 8-direction compass, 45 degrees a step.
        inline int FacingFromRotation(uint8_t rotation)
        {
            return (static_cast<int>(rotation) * (PlayerCamera::ANGLE_UNITS / 8)) & PlayerCamera::ANGLE_MASK;
        }

        // The bearing from an enemy to a point, in the 4096-unit angle space.
        // The original's own (FUN_00026b64, an arcsine table - up to four units
        // off a true atan2), since the attack waits for the facing to equal it.
        inline int BearingTo(const Enemy& enemy, float x, float z)
        {
            return ALTEngine::Game::OriginalMath::Bearing(
                static_cast<int>(std::lround(enemy.x - x)), static_cast<int>(std::lround(enemy.z - z)));
        }

        // Builds the roster for a level. Monsters parked off-map start inactive
        // - they are crate contents, and something has to let them out.
        inline std::vector<Enemy> Build(const ALTEngine::Formats::LevelGeometry& level,
                                        int difficulty,
                                        float originX, float originZ)
        {
            std::vector<Enemy> enemies;
            enemies.reserve(level.monsters.size());

            for (size_t i = 0; i < level.monsters.size(); ++i)
            {
                const auto& record = level.monsters[i];
                Enemy enemy;
                enemy.type = record.type;
                enemy.monsterIndex = static_cast<int>(i);
                enemy.baseHealth = record.health;
                enemy.health = SpawnHealth(record.type, record.health, difficulty);
                enemy.speed = record.speed;

                // The common task's spawn values (FUN_0002e638): the record's
                // speed word (bytes 14/15) with type 2's +0x14 on top, its wake
                // range (bytes 12/13 -> +0x50), and the per-type turn rate.
                enemy.ai.type = record.type;
                enemy.ai.speed = (record.speedHigh << 8 | record.speed)
                               + (record.type == 2 ? CreatureAI::HUGGER_SPEED_BONUS : 0);
                enemy.ai.wakeRange = record.wakeRange;
                enemy.ai.facing = enemy.ai.target = FacingFromRotation(record.rotation);
                enemy.drop = record.drop;
                enemy.triggerThreshold = record.triggerThreshold;
                enemy.facing = FacingFromRotation(record.rotation);

                // The record's own thresholds - bytes 18/19 and 16/17. No longer
                // guessed, and no longer needing a melee special case: a hugger's
                // are 768 and 1536 because that is what the level says.
                if (record.nearThreshold > 0) { enemy.nearThreshold = record.nearThreshold; }
                if (record.middleThreshold > 0) { enemy.middleThreshold = record.middleThreshold; }

                // FUN_0002e638 clamps two of the record's fields, for every type
                // but 0x10-0x13: the mode-1 threshold (+0x4a) to at least 0x600,
                // and the attack cone (+0x40, bytes 10/11) to 0x100..0x800, with
                // anything wider set to 0x700.
                if (EnemyBehaviour::ClampsRecordFields(record.type))
                {
                    if (enemy.nearThreshold < EnemyBehaviour::NEAR_THRESHOLD_MIN) { enemy.nearThreshold = EnemyBehaviour::NEAR_THRESHOLD_MIN; }
                    enemy.coneHalfWidth = EnemyBehaviour::ClampCone(record.coneHalfWidth);
                }

                // Cell centre, the same convention the crates and pickups use.
                enemy.x = static_cast<float>(record.x) * 512.0f + 256.0f - originX;
                enemy.z = static_cast<float>(record.y) * 512.0f + 256.0f - originZ;

                // Seed the movement reference to where it starts, or the first
                // tick reads as a move and every creature on the map registers as
                // a contact from the moment the level loads (Edward, 2026: "I can
                // still see enemies from ages away when I start the level").
                enemy.lastX = enemy.x;
                enemy.lastZ = enemy.z;
                enemy.y = ALTEngine::Formats::FindFloorHeightGridSpace(
                    level,
                    static_cast<int>(record.x) * 512 + 256,
                    static_cast<int>(record.y) * 512 + 256);

                // Threshold 0 is in play immediately; anything else waits for
                // whatever triggers it.
                enemy.active = ActiveAtStart(record);

                enemy.facingOctant = record.rotation & 7;

                // A colonist is pushed 0x40 out of its wall along its facing,
                // stood 0x40 over the floor, and holds the monster its speed
                // word names.
                if (record.type == 10)
                {
                    namespace CO = CreatureTasks::Colonist;
                    enemy.x += static_cast<float>(CO::PUSH_X[enemy.facingOctant] * CO::SPAWN_PUSH);
                    enemy.z += static_cast<float>(CO::PUSH_Z[enemy.facingOctant] * CO::SPAWN_PUSH);
                    enemy.lastX = enemy.x;
                    enemy.lastZ = enemy.z;
                    enemy.y += CO::SPAWN_LIFT;
                    enemy.heldMonster = record.speedHigh << 8 | record.speed;
                    enemy.animation = CO::SPAWN_ANIMATION;
                }

                // Vents face away from their record's heading and sit at the
                // height byte 6 gives them (FUN_0002e638's 0x10-0x13 arms).
                if (record.type >= 0x10 && record.type <= 0x13)
                {
                    const int gx = static_cast<int>(record.x) * 512 + 256;
                    const int gz = static_cast<int>(record.y) * 512 + 256;
                    const float floorY = ALTEngine::Formats::FindFloorHeightGridSpace(level, gx, gz);
                    const auto* cell = ALTEngine::Formats::CellAt(level, record.x, record.y);
                    const float ceilingY = cell ? static_cast<float>(cell->ceilingHeight) * 32.0f : floorY;
                    enemy.facing = (FacingFromRotation(record.rotation) + 0x800) & PlayerCamera::ANGLE_MASK;
                    enemy.ai.facing = enemy.ai.target = enemy.facing;
                    if (CreatureTasks::Vent::IsVertical(record.type))
                    {
                        const int lift = static_cast<int8_t>(record.drop);
                        enemy.y = lift >= 0 ? floorY + static_cast<float>(lift * 32)
                                            : ceilingY - static_cast<float>((10 - lift) * 32);
                        enemy.ventDown = lift < 0;
                    }
                    else
                    {
                        enemy.y = floorY + EnemyBehaviour::STAND_CLEARANCE + static_cast<float>(record.drop * 32);
                    }
                }

                // The queen starts in her waiting animation (FUN_0002e638).
                if (record.type == 7)
                {
                    enemy.animation = CreatureAI::QUEEN_ANIM_WAIT;
                    enemy.ai.queen = true;
                }

                // The egg's own thresholds replace the record's, and it holds
                // each animation's last frame.
                if (record.type == 1)
                {
                    enemy.nearThreshold = CreatureTasks::Egg::NEAR_THRESHOLD;
                    enemy.middleThreshold = CreatureTasks::Egg::MIDDLE_THRESHOLD;
                    enemy.holdAnimation = true;
                    enemy.animation = CreatureTasks::Egg::ANIM_CLOSED;
                }

                enemies.push_back(enemy);
            }
            return enemies;
        }

        // Lets a crate's monster out. Increments the trigger counter and only
        // activates once it reaches the threshold, as FUN_0002f288 does - and
        // moves the monster to the releasing object, since it has been parked
        // off the map until now.
        //
        // `facing` is the crate's rotation, which the original copies into the
        // monster's own two heading fields (<< 9) before starting the jump-out.
        inline bool Release(std::vector<Enemy>& enemies, int monsterIndex,
                            float x, float y, float z, int facing)
        {
            for (Enemy& enemy : enemies)
            {
                if (enemy.monsterIndex != monsterIndex) { continue; }
                if (enemy.active) { return false; }

                enemy.triggerCount++;
                if (enemy.triggerCount < enemy.triggerThreshold) { return false; }

                enemy.active = true;
                enemy.x = x;
                enemy.y = y;
                enemy.z = z;
                enemy.facing = (facing * (PlayerCamera::ANGLE_UNITS / 8)) & PlayerCamera::ANGLE_MASK;

                // The spring-out. Only subtypes 2 and 3 get it; anything else
                // simply appears.
                enemy.springing = (enemy.type == SPRING_SUBTYPE_A || enemy.type == SPRING_SUBTYPE_B);
                enemy.state = enemy.springing ? SPRING_STATE : Damage::STATE_NORMAL;
                return true;
            }
            return false;
        }

        // A trigger asking for a monster to appear, from FUN_0002f224: add to
        // its counter, clamp to its threshold, and spawn when they meet.
        //
        // The amount is a signed byte, and the count moves whether or not the
        // monster is already out: a negative one takes an active vent back
        // below its threshold, which switches it off (FUN_000358a4). A count
        // already at its threshold takes no positive amount, and a count
        // pushed past it - or wrapped below zero - is clamped to it.
        inline bool TriggerSpawn(std::vector<Enemy>& enemies, int monsterIndex, int amount)
        {
            const int delta = static_cast<int8_t>(amount & 0xff);
            for (Enemy& enemy : enemies)
            {
                if (enemy.monsterIndex != monsterIndex) { continue; }
                if (delta > 0 && enemy.triggerCount == enemy.triggerThreshold) { return false; }

                enemy.triggerCount = static_cast<uint8_t>(enemy.triggerCount + delta);
                if (enemy.triggerCount > enemy.triggerThreshold)
                {
                    enemy.triggerCount = enemy.triggerThreshold;
                }
                if (enemy.triggerCount != enemy.triggerThreshold || enemy.active) { return false; }

                enemy.active = true;
                return true;
            }
            return false;
        }

        // FUN_00035530: an egg lets its monster out. Unlike a trigger or a
        // crate it spawns the monster outright, at the egg, alerted and already
        // leaping at the player. `bearing` is the egg's bearing to the player.
        //
        // A colonist's burster goes through the same routine and then
        // FUN_00035624 sends it out harder - 0x60 along and 0x78 up - which
        // `speed` and `lift` carry. Returns the monster let out, or null.
        inline Enemy* ReleaseFromEgg(std::vector<Enemy>& enemies, int monsterIndex,
                                     float x, float y, float z, int bearing,
                                     int speed = CreatureTasks::EggRelease::SPEED,
                                     int lift = CreatureTasks::EggRelease::LIFT)
        {
            if (monsterIndex < 0 || monsterIndex == 0xff) { return nullptr; }
            for (Enemy& enemy : enemies)
            {
                if (enemy.monsterIndex != monsterIndex) { continue; }
                if (enemy.active) { return nullptr; }
                enemy.active = true;
                enemy.springing = false;
                enemy.state = Damage::STATE_NORMAL;
                enemy.x = enemy.lastX = x;
                enemy.y = y;
                enemy.z = enemy.lastZ = z;
                enemy.facing = bearing & PlayerCamera::ANGLE_MASK;
                enemy.ai.facing = enemy.ai.target = enemy.facing;
                enemy.ai.alerted = true;
                enemy.ai.idleTimer = 0;
                enemy.ai.state = CreatureAI::ACT;
                enemy.leaping = true;
                enemy.leapX = PlayerCamera::Sin(enemy.facing) / 4096.0f * static_cast<float>(speed);
                enemy.leapZ = -PlayerCamera::Cos(enemy.facing) / 4096.0f * static_cast<float>(speed);
                enemy.vy = static_cast<float>(lift);
                return &enemy;
            }
            return nullptr;
        }

        // Damage, through the same model the crates use - including the hit
        // window, which enemies genuinely have and objects do not.
        // Sixty ticks between attacks - EnemyBehaviour::ATTACK_TIMER_RELOAD,
        // traced. No longer a guess.
        inline constexpr int ATTACK_INTERVAL_TICKS = EnemyBehaviour::ATTACK_TIMER_RELOAD;

        // How long a frame is held. OURS - the real duration is entry 0 of the
        // animation's own sequence, which the VM reads; this stands in until the
        // sequences are wired through it.
        inline constexpr int ANIMATION_TICKS_PER_FRAME = 6;

        inline constexpr int HIT_REACTION_TICKS = 6;   // GUESS - the real one is
                                                       // the reaction animation's
                                                       // own length

        // `ignoreStun` is the Modern "Stunned Enemies" option: hits land during
        // the hit reaction as well, instead of the creature being immune until
        // it recovers.
        inline bool ApplyHit(Enemy& enemy, int damage, bool ignoreStun = false)
        {
            if (!enemy.alive || !enemy.active) { return false; }
            const bool stunned = enemy.state == Damage::STATE_HIT || enemy.hitCooldown != 0;
            if (!(ignoreStun && stunned) && !Damage::CanBeDamaged(enemy.state, enemy.hitCooldown, 0)) { return false; }

            enemy.health -= damage;
            if (enemy.health > 0)
            {
                enemy.state = Damage::STATE_HIT;
                enemy.hitCooldown = HIT_REACTION_TICKS;
                return false;
            }
            // Leave it alive for one more tick: the original checks health in
            // the tick's acting arm and moves to the dying state there, which is
            // what puts the death effect a tick later and at the body's final
            // position.
            enemy.state = EnemyBehaviour::STATE_DYING;
            return true;
        }

        // One entity tick, following FUN_000358a4's order: perception first, then
        // the distance cull, then the state machine.
        //
        // `playerX/Y/Z` are the player's world position. The thresholds come from
        // the entity's own fields in the original (+0x46 and +0x48); those are not
        // read from the level file and are not in the image as constants, so the
        // defaults below are OURS and marked.
        // The global pounce lock - DAT_000b0ab4's bit in the original. One
        // creature at a time, everywhere on the level.
        inline bool pounceLockHeld = false;

        // Per-level state that lives outside the roster. The pounce lock above
        // is only released when a pounce runs to completion, so dying or
        // quitting mid-pounce used to leave it held for the rest of the
        // session and no hugger could pounce again. Called on every level load.
        inline void ResetLevelState()
        {
            pounceLockHeld = false;
        }

        // Starts an animation, as FUN_0002f4b0 does - resetting the frame and the
        // timer. Re-starting the one already playing is ignored, which is what
        // stops a walk restarting every tick.
        inline void PlayAnimation(Enemy& enemy, int animation)
        {
            if (animation < 0 || enemy.animation == animation) { return; }
            enemy.animation = animation;
            enemy.animationFrame = 0;
            enemy.animationTimer = 0;
            enemy.animationChanged = true;
            enemy.animationFrames = 0;
            enemy.animationEnded = false;
        }

        // FUN_0002f4b0 itself, which starts the animation over even when it is
        // the one already playing.
        inline void RestartAnimation(Enemy& enemy, int animation)
        {
            if (animation < 0) { return; }
            enemy.animation = -1;
            PlayAnimation(enemy, animation);
        }

        // One tick of a vent's task (CreatureTasks::Vent).
        inline void RunVentTask(Enemy& enemy)
        {
            namespace VT = CreatureTasks::Vent;
            enemy.ventEmit = enemy.ventSound = enemy.ventBurst = enemy.ventSwitchedOff = false;
            enemy.stepX = enemy.stepZ = 0.0f;
            enemy.pendingDamage = 0;

            VT::Input in;
            in.type = enemy.type;
            in.cycleLength = enemy.middleThreshold;   // +0x48
            in.stopAt = enemy.nearThreshold;          // +0x4a
            in.stopLength = enemy.ai.speed;           // +0x5e
            in.triggeredOff = enemy.triggerCount < enemy.triggerThreshold;
            in.visible = enemy.zoneVisible;
            in.distance = enemy.distance;
            in.hasHealth = enemy.health > 0;

            // Shot apart: a pipe with health bursts and starts jetting.
            if (enemy.state == EnemyBehaviour::STATE_DYING || enemy.state == EnemyBehaviour::STATE_DEAD)
            {
                in.burst = enemy.vent.state == VT::DORMANT;
                enemy.health = 0;
                enemy.state = EnemyBehaviour::STATE_ACTING;
            }
            else if (enemy.state != EnemyBehaviour::STATE_ACTING)
            {
                // A hit (state 6) puts it back to dormant (0x359c2).
                enemy.state = EnemyBehaviour::STATE_ACTING;
                enemy.vent.state = VT::DORMANT;
            }

            const VT::Output out = VT::Step(enemy.vent, in);
            enemy.ventEmit = out.emit;
            enemy.ventSound = out.startSound;
            enemy.ventBurst = out.explode;
            enemy.ventSwitchedOff = out.switchOffNotice;
        }

        // One tick of a colonist's task (CreatureTasks::Colonist).
        inline void RunColonistTask(Enemy& enemy, float playerX, float playerZ, CreatureAI::World& world)
        {
            namespace CO = CreatureTasks::Colonist;
            CO::Input in;
            in.mode = enemy.mode;
            in.view = CO::ViewOf(enemy.facingOctant, BearingTo(enemy, playerX, playerZ));
            in.animationEvent = enemy.animationEnded;
            in.recoveredFromHit = enemy.recoveredFromHit;
            in.random = world.random ? world.random() : 0;
            enemy.recoveredFromHit = false;
            enemy.stepX = enemy.stepZ = 0.0f;
            enemy.pendingDamage = 0;

            const CO::Output out = CO::Step(enemy.colonist, in);
            RestartAnimation(enemy, out.startAnimation);
            enemy.cried = out.cry;
            if (out.burst)
            {
                enemy.releaseMonster = enemy.heldMonster;
                enemy.heldMonster = 0xff;
                if (enemy.drop != 0xff) { enemy.dropPickup = enemy.drop; }
            }
        }

        // One tick of the egg's task (CreatureTasks::Egg).
        inline void RunEggTask(Enemy& enemy, CreatureAI::World& world)
        {
            namespace EG = CreatureTasks::Egg;
            EG::Input in;
            in.mode = enemy.mode;
            in.lineToPlayer = enemy.lineOfFire;   // FUN_00032aec; the port has FUN_0003231c
            in.animationEnded = enemy.animationEnded;
            in.recoveredFromHit = enemy.recoveredFromHit;
            in.random = world.random ? world.random() : 0;
            enemy.recoveredFromHit = false;
            enemy.stepX = enemy.stepZ = 0.0f;
            enemy.pendingDamage = 0;

            const EG::Output out = EG::Step(enemy.egg, in);
            RestartAnimation(enemy, out.startAnimation);
            if (out.release)
            {
                enemy.releaseMonster = enemy.drop;
            }
            if (out.die)
            {
                enemy.health = 0;
                enemy.state = EnemyBehaviour::STATE_DYING;
            }
        }

        // Chooses the animation the creature's current behaviour calls for. The
        // three switches are traced - see EnemyBehaviour - and this is the routing
        // between them.
        inline void UpdateAnimation(Enemy& enemy)
        {
            if (!enemy.active) { return; }

            if (enemy.state == EnemyBehaviour::STATE_DYING
                || enemy.state == EnemyBehaviour::STATE_DEAD)
            {
                PlayAnimation(enemy, EnemyBehaviour::DeathAnimation(enemy.type));
                return;
            }
            if (enemy.state == EnemyBehaviour::STATE_HIT_RECOVER)
            {
                PlayAnimation(enemy, EnemyBehaviour::HitAnimation(enemy.type));
                return;
            }
            if (enemy.leaping)
            {
                // FUN_00033cbc starts animation 10 for a hugger's leap and 12 for
                // type 3's.
                PlayAnimation(enemy, enemy.type == 3 ? EnemyBehaviour::LEAP_ANIMATION_TYPE_3
                                                     : EnemyBehaviour::LEAP_ANIMATION_TYPE_2);
                return;
            }
            PlayAnimation(enemy, EnemyBehaviour::ActingAnimation(enemy.type));
        }

        // The vertical half of FUN_00031f3c, run after the creature has moved.
        // `floorY` and `ceilingY` are sampled under its new position.
        //
        //   - On the ground (no vertical velocity) it stands at floor + 0x20.
        //   - In the air it moves by its velocity. Reaching the ceiling (less its
        //     own half height) stops it there; dropping below floor + 0x20 lands
        //     it; otherwise gravity takes 0xc off, never leaving exactly 0, so a
        //     creature at the top of its arc still reads as airborne.
        //
        // The ceiling case is the original's: velocity 0 at the ceiling, and the
        // next tick puts it back on the floor.
        inline void SettleVertical(Enemy& enemy, float floorY, float ceilingY, float halfHeight)
        {
            namespace EB = EnemyBehaviour;
            float y = floorY + EB::STAND_CLEARANCE;
            if (enemy.vy != 0.0f)
            {
                const float next = enemy.y + enemy.vy;
                if (ceilingY - halfHeight < next)
                {
                    enemy.vy = 0.0f;
                    y = ceilingY - halfHeight;
                }
                else if (next < floorY + EB::STAND_CLEARANCE)
                {
                    enemy.vy = 0.0f;
                }
                else
                {
                    y = next;
                    enemy.vy -= EB::GRAVITY;
                    if (enemy.vy == 0.0f) { enemy.vy = -EB::GRAVITY; }
                }
            }
            enemy.y = y;
        }

        // The hit at the end of an attack (FUN_00033ff8): contact range, within
        // 0x600 vertically, in view, in a clear line and inside the cone.
        inline int ResolveAttack(const Enemy& enemy, float playerX, float playerZ)
        {
            int delta = (enemy.facing - BearingTo(enemy, playerX, playerZ)) & PlayerCamera::ANGLE_MASK;
            if (delta > PlayerCamera::ANGLE_UNITS / 2) { delta = PlayerCamera::ANGLE_UNITS - delta; }
            const bool inReach = enemy.mode == 0
                && enemy.separationY <= EnemyBehaviour::ATTACK_MAX_VERTICAL
                && enemy.zoneVisible && enemy.lineOfFire
                && EnemyBehaviour::WithinAttackCone(0, delta, enemy.coneHalfWidth);
            return inReach ? EnemyBehaviour::AttackDamage(enemy.type, enemy.mode) : 0;
        }

        // How long a swipe or spit plays before its hit lands. The original
        // waits for the attack animation's script to set +0xb6 bit 2; those
        // scripts are not read yet, so this stands in. GUESS.
        inline constexpr int NON_LEAP_ACT_TICKS = 0x18;

        // One tick of the common creature task for this creature (see
        // CreatureAI.h), in world coordinates.
        inline void RunCommonTask(Enemy& enemy, float playerX, float playerZ,
                                  CreatureAI::World& world, bool playerNoise)
        {
            namespace AI = CreatureAI;
            namespace EB = EnemyBehaviour;
            AI::Agent& a = enemy.ai;
            enemy.pendingDamage = 0;

            a.x = static_cast<int>(std::lround(enemy.x));
            a.z = static_cast<int>(std::lround(enemy.z));
            a.type = enemy.type;
            a.coneHalfWidth = enemy.coneHalfWidth;
            a.nearThreshold = enemy.nearThreshold;
            a.middleThreshold = enemy.middleThreshold;
            if (a.idleTimer > 0) { a.idleTimer--; }
            if (a.attackTimer > 0) { a.attackTimer--; }
            a.distance = enemy.distance;
            a.mode = enemy.mode;
            a.separationY = static_cast<int>(std::lround(enemy.separationY));
            a.bearingToPlayer = BearingTo(enemy, playerX, playerZ);
            a.lineToPlayer = enemy.lineOfFire;
            a.zoneVisible = enemy.zoneVisible;
            a.playerMakingNoise = playerNoise;

            a.queen = enemy.type == 7;
            a.animationEvent = enemy.animationEnded;

            // The queen's hit reaction ending is a state of its own (6).
            if (a.queen && enemy.recoveredFromHit)
            {
                enemy.recoveredFromHit = false;
                a.startAnimation = -1;
                AI::QueenRecover(a, world);
                enemy.facing = a.facing;
                RestartAnimation(enemy, a.startAnimation);
                enemy.stepX = enemy.stepZ = 0.0f;
                return;
            }
            enemy.recoveredFromHit = false;

            // Has the act ended: a leap on landing, a swipe when it has played.
            if (a.state == AI::ACT)
            {
                if (enemy.leaping) { a.actFinished = (enemy.vy == 0.0f); }
                else if (enemy.actTicks > 0) { a.actFinished = (--enemy.actTicks == 0); }
                else { a.actFinished = true; }
                if (a.actFinished && enemy.leaping)
                {
                    enemy.leaping = false;
                    enemy.leapX = enemy.leapZ = 0.0f;
                }
            }

            AI::Step(a, world, static_cast<int>(std::lround(playerX)), static_cast<int>(std::lround(playerZ)));
            enemy.facing = a.facing;
            RestartAnimation(enemy, a.startAnimation);

            if (a.resolveHit)
            {
                enemy.pendingDamage = ResolveAttack(enemy, playerX, playerZ);
                enemy.velocityActive = false;
            }

            if (a.startAct)
            {
                // FUN_00033cbc's per-type arms.
                const bool melee = EB::AttackForSubtype(enemy.type).melee;
                if (enemy.type == 2 && enemy.mode == 0
                    && enemy.separationY <= EB::FACE_HUG_MAX_VERTICAL && !pounceLockHeld)
                {
                    pounceLockHeld = true;
                    enemy.pouncing = true;
                    enemy.pounceTicks = EB::POUNCE_HANDOVER_TIMER;
                    enemy.pendingDamage = EB::AttackForSubtype(enemy.type).closeDamage;
                    enemy.stepX = enemy.stepZ = 0.0f;
                    a.vx = a.vz = 0;
                    return;
                }
                if (melee)
                {
                    enemy.leaping = true;
                    enemy.leapX = PlayerCamera::Sin(enemy.facing) / 4096.0f * EB::LEAP_SPEED;
                    enemy.leapZ = -PlayerCamera::Cos(enemy.facing) / 4096.0f * EB::LEAP_SPEED;
                    if (enemy.vy == 0.0f) { enemy.vy = EB::LEAP_LIFT; }
                }
                else
                {
                    enemy.actTicks = NON_LEAP_ACT_TICKS;
                }
            }

            if (enemy.leaping && a.state == AI::ACT)
            {
                enemy.stepX = enemy.leapX;
                enemy.stepZ = enemy.leapZ;
            }
            else
            {
                enemy.stepX = static_cast<float>(a.vx);
                enemy.stepZ = static_cast<float>(a.vz);
            }
        }

        inline void Tick(Enemy& enemy, float playerX, float playerY, float playerZ,
                         CreatureAI::World* world = nullptr, bool playerNoise = false)
        {
            // The queen's task is the common one with her own differences
            // (CreatureAI.h).
            const bool commonTask = world != nullptr
                && (CreatureAI::UsesCommonTask(enemy.type) || enemy.type == 7);
            const bool eggTask = world != nullptr && enemy.type == 1;
            const bool ventTask = world != nullptr && enemy.type >= 0x10 && enemy.type <= 0x13;
            const bool colonistTask = world != nullptr && enemy.type == 10;
            // Types with a task of their own don't walk at the player.
            const bool ownTask = commonTask || eggTask || colonistTask;
            enemy.releaseMonster = -1;
            enemy.dropPickup = -1;
            enemy.cried = false;
            if (!enemy.active) { return; }

            // Perception - the axis separations and the banded distance.
            enemy.separationX = std::fabs(playerX - enemy.x);
            enemy.separationY = std::fabs(playerY - enemy.y);
            enemy.separationZ = std::fabs(playerZ - enemy.z);
            // FUN_000320f8's distance, with its table square root.
            enemy.distance = ALTEngine::Game::OriginalMath::Distance(
                static_cast<int>(std::lround(playerX - enemy.x)), static_cast<int>(std::lround(playerZ - enemy.z)));

            enemy.mode = EnemyBehaviour::ModeForDistance(
                enemy.distance, enemy.nearThreshold, enemy.middleThreshold);

            // A vent's task is the whole of it: it never moves, attacks or
            // animates, and its own cycle runs at any distance.
            if (ventTask)
            {
                RunVentTask(enemy);
                return;
            }

            // NOT SIMULATED BEYOND THIS. FUN_000358a4 returns immediately when the
            // perceived distance exceeds 0x1e00, so an enemy across the level does
            // nothing at all - it does not path, does not turn, does not count
            // down. Worth having: without it every enemy on the map would be
            // walking toward the player from the moment the level loads.
            //
            // That cutoff is the other tasks'. The common task (CreatureAI.h)
            // runs at any distance and only stands still beyond 0x3fff (mode 4,
            // 0x3431b).
            if (commonTask)
            {
                if (enemy.mode == 4) { enemy.stepX = enemy.stepZ = 0.0f; enemy.ai.vx = enemy.ai.vz = 0; return; }
            }
            else if (!eggTask && !colonistTask && enemy.distance > EnemyBehaviour::SIMULATION_CUTOFF) { return; }

            enemy.ticks++;

            // Pick the animation for whatever the creature is doing, then advance
            // it. The frame duration comes from the sequence in the original; a
            // fixed rate stands in until the sequences are fed through the VM.
            UpdateAnimation(enemy);
            if (++enemy.animationTimer >= ANIMATION_TICKS_PER_FRAME)
            {
                enemy.animationTimer = 0;
                enemy.animationFrame++;
            }
            // Played through. Without a frame count from the sprites, four
            // frames stand in.
            {
                const int frames = enemy.animationFrames > 0 ? enemy.animationFrames : 4;
                if (enemy.animationFrame >= frames)
                {
                    enemy.animationEnded = true;
                    if (enemy.holdAnimation) { enemy.animationFrame = frames - 1; }
                }
            }

            // ---- movement, from FUN_00031f3c --------------------------
            //
            // Turn toward the player, then step. The original stores a heading
            // and two axis velocities and lets the two step helpers resolve each
            // axis separately; this does the same, using the traced footprint
            // probe for each.
            if (!ownTask && enemy.state == EnemyBehaviour::STATE_ACTING && enemy.alive && enemy.mode <= 2)
            {
                const int bearing = BearingTo(enemy, playerX, playerZ);

                // Turn at a limited rate rather than snapping - the original
                // turns through FUN_00032f78 before starting a move animation.
                int delta = (bearing - enemy.facing) & PlayerCamera::ANGLE_MASK;
                if (delta > PlayerCamera::ANGLE_UNITS / 2) { delta -= PlayerCamera::ANGLE_UNITS; }
                const int turn = (delta > TURN_RATE) ? TURN_RATE
                               : (delta < -TURN_RATE) ? -TURN_RATE : delta;
                enemy.facing = (enemy.facing + turn) & PlayerCamera::ANGLE_MASK;

                // TURN FIRST, THEN ADVANCE. Stepping along the heading while
                // still turning made a creature walk AWAY from the player for the
                // first twenty ticks - it kept its old facing, moved backwards,
                // and only curved round once the turn caught up. Requiring rough
                // alignment before moving fixes that.
                //
                // MARKED AS OURS. The original steers through FUN_00032f78 and
                // keeps its velocities in separate fields, so it may well move
                // and turn at once with a much faster turn; without those fields
                // read, turn-then-advance is the behaviour that does not look
                // broken.
                const bool aligned = (delta > -ALIGN_BEFORE_MOVING && delta < ALIGN_BEFORE_MOVING);

                // Only close in while not already touching. Mode 0 is contact
                // range, where the creature attacks instead of advancing.
                // Mode 0 is CONTACT range - 0x400, and that is where the
                // creature stops advancing and attacks instead. Note this leaves
                // it a full 0x400 (1024 units) away, which looks like a standoff
                // rather than a mauling; the original almost certainly closes
                // further, and the gap is probably the entity's own body radius
                // being subtracted somewhere I have not read. MARKED.
                if (enemy.mode >= 1 && aligned)
                {
                    const float speed = static_cast<float>(enemy.speed) * SPEED_SCALE;
                    enemy.stepX = PlayerCamera::Sin(enemy.facing) / 4096.0f * speed;
                    enemy.stepZ = -PlayerCamera::Cos(enemy.facing) / 4096.0f * speed;
                }
                else
                {
                    enemy.stepX = enemy.stepZ = 0.0f;
                }
            }
            else
            {
                enemy.stepX = enemy.stepZ = 0.0f;
            }

            // ---- the pounce -------------------------------------------
            //
            // A face hugger inside POUNCE_RANGE leaps instead of standing there
            // clawing. FUN_00033cbc's subtype 2 arm checks the range, takes a
            // GLOBAL exclusivity bit so only one creature can be doing it at a
            // time, and then REPLACES the creature's handler with LAB_00030918 -
            // it leaves the ordinary state machine completely.
            //
            // That is what was wrong: without it a hugger reached contact range,
            // stopped, and stayed in the attack path landing a hit every
            // ATTACK_INTERVAL_TICKS forever (Edward, 2026: "they still just sit at
            // your feet and do too much damage, they also don't leap").
            if (enemy.pouncing)
            {
                // Out of the normal machine while the leap plays, and at the end
                // of it the creature DIES - LAB_00030918 calls the death function
                // outright. A hugger gets exactly one leap.
                enemy.pendingDamage = 0;
                enemy.stepX = enemy.stepZ = 0.0f;

                // The creature is spent when the CLIMB finishes, not on a timer of
                // its own - the two were running independently and drifting apart
                // (Edward, 2026: "it all happens a bit out of time"). The overlay
                // owns the clock; this follows it.
                if (enemy.overlayFinished || --enemy.pounceTicks <= 0)
                {
                    enemy.pouncing = false;
                    pounceLockHeld = false;

                    // Lands next to the player and expires there, as
                    // LAB_00030918 does.
                    enemy.x = playerX;
                    enemy.z = playerZ;
                    enemy.state = EnemyBehaviour::STATE_DYING;
                    enemy.health = 0;
                }
                return;
            }

            if (eggTask && enemy.state == EnemyBehaviour::STATE_ACTING && enemy.alive)
            {
                RunEggTask(enemy, *world);
            }
            else if (colonistTask && enemy.state == EnemyBehaviour::STATE_ACTING && enemy.alive)
            {
                RunColonistTask(enemy, playerX, playerZ, *world);
            }
            else if (commonTask && enemy.state == EnemyBehaviour::STATE_ACTING && enemy.alive)
            {
                RunCommonTask(enemy, playerX, playerZ, *world, playerNoise);
            }
            else
            // ---- melee creatures: the leap ------------------------------
            //
            // Types 2 and 3 never claw from where they stand. Their attack is
            // FUN_00033cbc: a face hug, or a leap at the player whose damage
            // lands with it. Either needs the creature to have closed to
            // contact range first. See LEAP_* in EnemyBehaviour.h.
            if (EnemyBehaviour::AttackForSubtype(enemy.type).melee
                && enemy.state == EnemyBehaviour::STATE_ACTING
                && enemy.alive)
            {
                namespace EB = EnemyBehaviour;
                enemy.pendingDamage = 0;

                if (enemy.leaping)
                {
                    if (enemy.vy != 0.0f)
                    {
                        // Still in the air: the take-off velocity carries it.
                        enemy.stepX = enemy.leapX;
                        enemy.stepZ = enemy.leapZ;
                    }
                    else
                    {
                        // Down. Velocities cleared, then FUN_00033ff8 - the same
                        // gates as any other attack: contact range, within reach
                        // vertically, and facing the player.
                        enemy.leaping = false;
                        enemy.leapX = enemy.leapZ = 0.0f;

                        // A hugger goes back to the chase after its leap
                        // (0x348f9: state 2); type 3 stays in the attack state.
                        if (enemy.type == 2) { enemy.engaged = false; }
                        enemy.stepX = enemy.stepZ = 0.0f;
                        enemy.velocityActive = false;
                        int delta = (enemy.facing - BearingTo(enemy, playerX, playerZ)) & PlayerCamera::ANGLE_MASK;
                        if (delta > PlayerCamera::ANGLE_UNITS / 2) { delta = PlayerCamera::ANGLE_UNITS - delta; }
                        if (enemy.mode == 0
                            && enemy.separationY <= EB::ATTACK_MAX_VERTICAL
                            && EB::WithinAttackCone(0, delta, enemy.coneHalfWidth))
                        {
                            enemy.pendingDamage = EB::AttackDamage(enemy.type, enemy.mode);
                        }
                    }
                }
                else
                {
                    // THE ATTACK STATE IS ENTERED FROM CONTACT RANGE ONLY. The
                    // chase (state 2, 0x3445e) moves to state 4 when the mode is
                    // 0 and the creature is within 0x600 vertically; state 4
                    // attacks from any mode below 3 and gives up at mode 3.
                    if (enemy.mode >= EB::FAR_MODE) { enemy.engaged = false; }
                    else if (enemy.mode == 0 && enemy.separationY <= EB::FACE_HUG_MAX_VERTICAL && enemy.lineOfFire)
                    {
                        enemy.engaged = true;
                    }

                    if (enemy.attackCooldown > 0) { enemy.attackCooldown--; }
                    const bool facingPlayer = enemy.facing == BearingTo(enemy, playerX, playerZ);
                    if (enemy.engaged
                        && enemy.lineOfFire
                        && enemy.attackCooldown == 0
                        && facingPlayer
                        && enemy.mode < EB::FAR_MODE
                        && enemy.separationY <= EB::ATTACK_REACH_VERTICAL)
                    {
                        enemy.attackCooldown = ATTACK_INTERVAL_TICKS;

                        if (enemy.type == 2
                            && enemy.mode == 0
                            && enemy.separationY <= EB::FACE_HUG_MAX_VERTICAL
                            && !pounceLockHeld)
                        {
                            pounceLockHeld = true;
                            enemy.pouncing = true;
                            enemy.pounceTicks = EB::POUNCE_HANDOVER_TIMER;

                            // The face hug's own hit, landed ONCE at the start.
                            // A hugger that gets on your face deals its damage
                            // and is done.
                            enemy.pendingDamage = EB::AttackForSubtype(enemy.type).closeDamage;
                            enemy.stepX = enemy.stepZ = 0.0f;
                            return;
                        }

                        enemy.leaping = true;
                        enemy.leapX = PlayerCamera::Sin(enemy.facing) / 4096.0f * EB::LEAP_SPEED;
                        enemy.leapZ = -PlayerCamera::Cos(enemy.facing) / 4096.0f * EB::LEAP_SPEED;
                        if (enemy.vy == 0.0f) { enemy.vy = EB::LEAP_LIFT; }
                        enemy.stepX = enemy.leapX;
                        enemy.stepZ = enemy.leapZ;
                    }
                }
            }
            else
            {
                // The attack, from FUN_00033ff8. Two gates in the original - "can it
                // attack" and "is the player in front" - stand in here as a cooldown
                // and a facing test, both marked, since neither FUN_00033a1c nor
                // FUN_0003231c has been read.
                enemy.pendingDamage = 0;
                if (enemy.state == EnemyBehaviour::STATE_ACTING && enemy.alive)
                {
                    if (enemy.attackCooldown > 0) { enemy.attackCooldown--; }
                    else
                    {
                        // The facing cone, the zone test (FUN_00033a1c) and the
                        // line of fire (FUN_0003231c) - all three, or no attack.
                        const int bearing = static_cast<int>(std::lround(
                            std::atan2(playerX - enemy.x, -(playerZ - enemy.z))
                            * (PlayerCamera::ANGLE_UNITS / 6.28318530718)))
                            & PlayerCamera::ANGLE_MASK;

                        int delta = ((enemy.facing - bearing) & PlayerCamera::ANGLE_MASK);
                        if (delta > PlayerCamera::ANGLE_UNITS / 2) { delta = PlayerCamera::ANGLE_UNITS - delta; }

                        // CONTACT RANGE ONLY, and within 0x600 vertically. Both are
                        // the original's own gates, and both were missing - a creature
                        // could reach from the middle band and from any height.
                        const bool inReach = (enemy.mode == 0)
                            && (enemy.separationY <= EnemyBehaviour::ATTACK_MAX_VERTICAL)
                            && enemy.zoneVisible && enemy.lineOfFire;

                        const int damage = (inReach
                            && EnemyBehaviour::WithinAttackCone(0, delta, enemy.coneHalfWidth))
                            ? EnemyBehaviour::AttackDamage(enemy.type, enemy.mode)
                            : 0;
                        if (damage > 0)
                        {
                            enemy.pendingDamage = damage;
                            enemy.attackCooldown = ATTACK_INTERVAL_TICKS;

                            // Attacking clears the velocity, so the contact goes dark
                            // until the creature moves again - see the note in
                            // HudPanel.h.
                            enemy.velocityActive = false;
                        }
                    }
                }

            }

            switch (enemy.state)
            {
            case EnemyBehaviour::STATE_ACTING:
                // Death is checked here, not at the moment of the hit.
                if (enemy.health <= 0)
                {
                    enemy.state = EnemyBehaviour::STATE_DEAD;
                    enemy.alive = false;
                    enemy.ticks = 0;
                }
                break;

            case EnemyBehaviour::STATE_HIT_RECOVER:
                // The reaction has played out: back to acting.
                enemy.state = EnemyBehaviour::STATE_ACTING;
                enemy.recoveredFromHit = true;
                enemy.ticks = 0;
                break;

            case EnemyBehaviour::STATE_DYING:
                // The original spawns the death effect here rather than on the
                // killing blow, so it lands a tick later and at the position the
                // body ended up at.
                enemy.spawnDeathEffect = true;
                if (enemy.type == 7 && enemy.drop != 0xff)
                {
                    enemy.deathChain = enemy.drop;
                    enemy.drop = 0xff;
                }
                enemy.state = EnemyBehaviour::STATE_DEAD;
                enemy.alive = false;
                break;

            case EnemyBehaviour::STATE_DEAD:
                break;

            default:
                // Anything unexpected drops to the hit reaction, as the original's
                // default arm does.
                enemy.state = EnemyBehaviour::STATE_HIT_RECOVER;
                break;
            }
        }
    }
}
