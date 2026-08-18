#pragma once

#include "../Formats/LevelLoader.h"
#include "../Formats/SpriteAnimator.h"
#include "DamageSystem.h"
#include "EnemyBehaviour.h"
#include "PlayerCamera.h"

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


        // A melee creature's bands. 0x400 is the original's own contact range
        // (MODE_TOUCHING_RANGE), so a hugger is at mode 0 while touching, mode 1
        // just outside, and mode 2 barely beyond that - which is as far as its
        // 1-damage nibble can land. GUESSES, but bounded by a traced number.
        inline constexpr int MELEE_NEAR_THRESHOLD = 0x500;
        inline constexpr int MELEE_MIDDLE_THRESHOLD = 0x600;

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

            // The attack cone half-width, the original's +0x40. Also from the
            // entity template, so also a GUESS - 0x200 of 4096 is about 17
            // degrees either side.
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
        inline int BearingTo(const Enemy& enemy, float x, float z)
        {
            return static_cast<int>(std::lround(
                std::atan2(x - enemy.x, -(z - enemy.z))
                * (PlayerCamera::ANGLE_UNITS / 6.28318530718))) & PlayerCamera::ANGLE_MASK;
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
                enemy.drop = record.drop;
                enemy.triggerThreshold = record.triggerThreshold;
                enemy.facing = FacingFromRotation(record.rotation);

                // The record's own thresholds - bytes 18/19 and 16/17. No longer
                // guessed, and no longer needing a melee special case: a hugger's
                // are 768 and 1536 because that is what the level says.
                if (record.nearThreshold > 0) { enemy.nearThreshold = record.nearThreshold; }
                if (record.middleThreshold > 0) { enemy.middleThreshold = record.middleThreshold; }

                // Cell centre, the same convention the crates and pickups use.
                enemy.x = static_cast<float>(record.x) * 512.0f + 256.0f - originX;
                enemy.z = static_cast<float>(record.y) * 512.0f + 256.0f - originZ;
                enemy.y = ALTEngine::Formats::FindFloorHeightGridSpace(
                    level,
                    static_cast<int>(record.x) * 512 + 256,
                    static_cast<int>(record.y) * 512 + 256);

                // Threshold 0 is in play immediately; anything else waits for
                // whatever triggers it.
                enemy.active = ActiveAtStart(record);

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
        inline bool TriggerSpawn(std::vector<Enemy>& enemies, int monsterIndex, int amount)
        {
            for (Enemy& enemy : enemies)
            {
                if (enemy.monsterIndex != monsterIndex || enemy.active) { continue; }

                enemy.triggerCount = static_cast<uint8_t>(enemy.triggerCount + amount);
                if (enemy.triggerCount > enemy.triggerThreshold)
                {
                    enemy.triggerCount = enemy.triggerThreshold;
                }
                if (enemy.triggerCount != enemy.triggerThreshold) { return false; }

                enemy.active = true;
                return true;
            }
            return false;
        }

        // Damage, through the same model the crates use - including the hit
        // window, which enemies genuinely have and objects do not.
        // How often a creature can land a hit. GUESS - the original gates this
        // with FUN_00033a1c, which has not been read.
        inline constexpr int ATTACK_INTERVAL_TICKS = 30;

        inline constexpr int HIT_REACTION_TICKS = 6;   // GUESS - the real one is
                                                       // the reaction animation's
                                                       // own length

        inline bool ApplyHit(Enemy& enemy, int damage)
        {
            if (!enemy.alive || !enemy.active) { return false; }
            if (!Damage::CanBeDamaged(enemy.state, enemy.hitCooldown, 0)) { return false; }

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
        inline void Tick(Enemy& enemy, float playerX, float playerY, float playerZ)
        {
            if (!enemy.active) { return; }

            // Perception - the axis separations and the banded distance.
            enemy.separationX = std::fabs(playerX - enemy.x);
            enemy.separationY = std::fabs(playerY - enemy.y);
            enemy.separationZ = std::fabs(playerZ - enemy.z);
            const float dx = playerX - enemy.x;
            const float dz = playerZ - enemy.z;
            enemy.distance = static_cast<int>(std::lround(std::sqrt(dx * dx + dz * dz)));

            enemy.mode = EnemyBehaviour::ModeForDistance(
                enemy.distance, enemy.nearThreshold, enemy.middleThreshold);

            // NOT SIMULATED BEYOND THIS. FUN_000358a4 returns immediately when the
            // perceived distance exceeds 0x1e00, so an enemy across the level does
            // nothing at all - it does not path, does not turn, does not count
            // down. Worth having: without it every enemy on the map would be
            // walking toward the player from the moment the level loads.
            if (enemy.distance > EnemyBehaviour::SIMULATION_CUTOFF) { return; }

            enemy.ticks++;

            // ---- movement, from FUN_00031f3c --------------------------
            //
            // Turn toward the player, then step. The original stores a heading
            // and two axis velocities and lets the two step helpers resolve each
            // axis separately; this does the same, using the traced footprint
            // probe for each.
            if (enemy.state == EnemyBehaviour::STATE_ACTING && enemy.alive && enemy.mode <= 2)
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
                    // The facing cone is the original's own gate; the visibility
                    // and line-of-fire tests are not implemented yet, so a
                    // creature here will attack through a wall it should not.
                    const int bearing = static_cast<int>(std::lround(
                        std::atan2(playerX - enemy.x, -(playerZ - enemy.z))
                        * (PlayerCamera::ANGLE_UNITS / 6.28318530718)))
                        & PlayerCamera::ANGLE_MASK;

                    int delta = ((enemy.facing - bearing) & PlayerCamera::ANGLE_MASK);
                    if (delta > PlayerCamera::ANGLE_UNITS / 2) { delta = PlayerCamera::ANGLE_UNITS - delta; }

                    const int damage = EnemyBehaviour::WithinAttackCone(0, delta, enemy.coneHalfWidth)
                                     ? EnemyBehaviour::AttackDamage(enemy.type, enemy.mode)
                                     : 0;
                    if (damage > 0)
                    {
                        enemy.pendingDamage = damage;
                        enemy.attackCooldown = ATTACK_INTERVAL_TICKS;
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
                enemy.ticks = 0;
                break;

            case EnemyBehaviour::STATE_DYING:
                // The original spawns the death effect here rather than on the
                // killing blow, so it lands a tick later and at the position the
                // body ended up at.
                enemy.spawnDeathEffect = true;
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
