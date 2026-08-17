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
        // Which monsters a difficulty setting actually places.
        //
        // The record's difficulty byte is 0 Easy, 1 Medium, 2 Hard. A monster
        // tagged 0 is present on every setting, 1 on medium and hard, 2 on hard
        // only - which is the reading that matches L111 having 24 type-2
        // monsters spread across all three values rather than three separate
        // rosters. MARKED: the gating code itself has not been traced.
        inline bool PresentAtDifficulty(int recordDifficulty, int selected)
        {
            return recordDifficulty <= selected;
        }

        // A monster parked off the playable map, waiting to be let out of a
        // crate. On L111 every crate-held monster sits at x = 9 in a row at
        // y = 83/85/87/89, well outside the rooms.
        inline constexpr int PARKED_X = 9;

        struct Enemy
        {
            int type = 0;
            int monsterIndex = -1;      // its record in level.monsters

            float x = 0, y = 0, z = 0;
            int facing = 0;             // 4096-per-turn, from the 8-direction byte

            int health = 0;
            int speed = 0;
            int state = Damage::STATE_NORMAL;
            int hitCooldown = 0;
            uint8_t drop = 0xFF;        // what it leaves behind

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
                                        int selectedDifficulty,
                                        float originX, float originZ)
        {
            std::vector<Enemy> enemies;
            enemies.reserve(level.monsters.size());

            for (size_t i = 0; i < level.monsters.size(); ++i)
            {
                const auto& record = level.monsters[i];
                if (!PresentAtDifficulty(record.difficulty, selectedDifficulty)) { continue; }

                Enemy enemy;
                enemy.type = record.type;
                enemy.monsterIndex = static_cast<int>(i);
                enemy.health = record.health;
                enemy.speed = record.speed;
                enemy.drop = record.drop;
                enemy.facing = FacingFromRotation(record.rotation);

                // Cell centre, the same convention the crates and pickups use.
                enemy.x = static_cast<float>(record.x) * 512.0f + 256.0f - originX;
                enemy.z = static_cast<float>(record.y) * 512.0f + 256.0f - originZ;
                enemy.y = ALTEngine::Formats::FindFloorHeightGridSpace(
                    level,
                    static_cast<int>(record.x) * 512 + 256,
                    static_cast<int>(record.y) * 512 + 256);

                // Parked monsters are crate contents and stay out of play until
                // released.
                enemy.active = (record.x != PARKED_X);

                enemies.push_back(enemy);
            }
            return enemies;
        }

        // Lets a crate's monster out at the crate's position.
        inline bool Release(std::vector<Enemy>& enemies, int monsterIndex,
                            float x, float y, float z)
        {
            for (Enemy& enemy : enemies)
            {
                if (enemy.monsterIndex != monsterIndex || enemy.active) { continue; }
                enemy.active = true;
                enemy.x = x;
                enemy.y = y;
                enemy.z = z;
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
