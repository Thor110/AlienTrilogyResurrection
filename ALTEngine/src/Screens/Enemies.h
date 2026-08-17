#pragma once

#include "../Formats/LevelLoader.h"
#include "../Formats/SpriteAnimator.h"
#include "DamageSystem.h"
#include "PlayerCamera.h"

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

            bool active = false;        // released and in play
            bool alive = true;

            ALTEngine::Formats::SpriteAnim::Animator animator;
        };

        // The record's rotation is an 8-direction compass, 45 degrees a step.
        inline int FacingFromRotation(uint8_t rotation)
        {
            return (static_cast<int>(rotation) * (PlayerCamera::ANGLE_UNITS / 8)) & PlayerCamera::ANGLE_MASK;
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

        // Damage, through the same model the crates use - including the hit
        // window, which enemies genuinely have and objects do not.
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
            enemy.state = Damage::STATE_DYING;
            enemy.alive = false;
            return true;
        }

        inline void Tick(Enemy& enemy)
        {
            if (enemy.hitCooldown > 0 && --enemy.hitCooldown == 0)
            {
                if (enemy.alive) { enemy.state = Damage::STATE_NORMAL; }
            }
        }
    }
}
