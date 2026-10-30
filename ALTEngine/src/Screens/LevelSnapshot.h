#pragma once

#include "../Formats/LevelLoader.h"
#include "../Game/Doors.h"
#include "../Game/Lifts.h"
#include "DamageSystem.h"
#include "Enemies.h"
#include "PlayerCamera.h"
#include "PlayerHudState.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace ALTEngine::Screens::LevelSnapshot
{
    // MID-LEVEL SAVES (Modern: Save Anywhere). The level as it stands, as JSON,
    // for the save file's "snapshot" section. Loading builds the level as
    // usual from its files and then puts this over it.
    //
    // What is kept is what play changes: the cell grid (doors, lifts, marks,
    // anything a trigger rewrote), each door, lift, trigger, pickup, crate and
    // switch, every creature, the player and the HUD. What isn't kept is
    // what lasts a moment: rounds and other particles in flight, explosions,
    // debris, sounds, and what a creature was in the middle of. A creature
    // comes back where it was, with its health, alerted if it had been, and
    // decides afresh what to do.
    //
    // Each piece is a pair of functions here, so the tests can check it
    // comes back the same; GameplayScreen puts the pieces together.

    inline constexpr int VERSION = 1;

    // ---- the cell grid --------------------------------------------------

    // Sixteen bytes a cell, as hex: every byte of every cell, since doors,
    // lifts, object and creature marks and trigger commands all write there.
    inline std::string GridToHex(const std::vector<Formats::CollisionNode>& grid)
    {
        static const char* digits = "0123456789abcdef";
        std::string out;
        out.reserve(grid.size() * 32);
        for (const auto& c : grid)
        {
            const uint8_t b[16] = { c.unknown1, c.unknown2, c.unknown3, c.unknown4, c.unknown5, c.unknown6,
                                    c.unknown7, c.unknown8, c.ceilingFog, c.floorFog, c.attribute, c.floorHeight,
                                    c.unknown13, c.ceilingHeight, c.lighting, c.scriptAction };
            for (uint8_t v : b) { out.push_back(digits[v >> 4]); out.push_back(digits[v & 15]); }
        }
        return out;
    }

    // False, and the grid untouched, if the hex isn't this grid's size.
    inline bool GridFromHex(std::vector<Formats::CollisionNode>& grid, const std::string& hex)
    {
        if (hex.size() != grid.size() * 32) { return false; }
        auto nibble = [](char ch) -> int {
            if (ch >= '0' && ch <= '9') { return ch - '0'; }
            if (ch >= 'a' && ch <= 'f') { return ch - 'a' + 10; }
            if (ch >= 'A' && ch <= 'F') { return ch - 'A' + 10; }
            return -1;
        };
        for (char ch : hex) { if (nibble(ch) < 0) { return false; } }
        size_t pos = 0;
        auto next = [&]() { const int v = nibble(hex[pos]) << 4 | nibble(hex[pos + 1]); pos += 2; return static_cast<uint8_t>(v); };
        for (auto& c : grid)
        {
            c.unknown1 = next(); c.unknown2 = next(); c.unknown3 = next(); c.unknown4 = next();
            c.unknown5 = next(); c.unknown6 = next(); c.unknown7 = next(); c.unknown8 = next();
            c.ceilingFog = next(); c.floorFog = next(); c.attribute = next(); c.floorHeight = next();
            c.unknown13 = next(); c.ceilingHeight = next(); c.lighting = next(); c.scriptAction = next();
        }
        return true;
    }

    // ---- doors and lifts --------------------------------------------------

    // A door's moving parts. Where it is and which cells it has come from
    // the level's own records, rebuilt on load.
    inline nlohmann::json SaveDoor(const Game::DoorState& d)
    {
        return { { "progress", d.progress }, { "counter", d.counter }, { "phase", static_cast<int>(d.phase) },
                 { "unlockProgress", d.unlockProgress }, { "switchOperated", d.switchOperated },
                 { "isOpen", d.isOpen }, { "holdCounter", d.holdCounter } };
    }

    inline void LoadDoor(Game::DoorState& d, const nlohmann::json& j)
    {
        d.progress = std::clamp(j.value("progress", d.progress), 0, d.travel);
        // Saves from before the counter was kept used progress for both.
        d.counter = std::clamp(j.value("counter", d.progress), 0, d.travel);
        d.phase = static_cast<Game::DoorState::Phase>(std::clamp(j.value("phase", 0), 0, 4));
        d.unlockProgress = j.value("unlockProgress", d.unlockProgress);
        d.switchOperated = j.value("switchOperated", d.switchOperated);
        d.isOpen = j.value("isOpen", d.phase == Game::DoorState::Phase::Open);
        d.holdCounter = j.value("holdCounter", j.value("cooldown", d.holdCounter));
    }

    inline nlohmann::json SaveLift(const Game::Lifts::Lift& l)
    {
        return { { "flags", l.flags }, { "dwellCounter", l.dwellCounter }, { "counter", l.counter },
                 { "height", l.height }, { "phase", static_cast<int>(l.phase) } };
    }

    inline void LoadLift(Game::Lifts::Lift& l, const nlohmann::json& j)
    {
        l.flags = j.value("flags", l.flags);
        l.dwellCounter = j.value("dwellCounter", l.dwellCounter);
        l.counter = j.value("counter", l.counter);
        l.height = j.value("height", l.height);
        l.phase = static_cast<Game::Lifts::Phase>(j.value("phase", static_cast<int>(l.phase)));
    }

    // ---- crates, barrels and the other shootable objects --------------------

    inline nlohmann::json SaveTarget(const Damage::Target& t)
    {
        return { { "health", t.health }, { "destroyed", t.destroyed }, { "fuse", t.fuse } };
    }

    // Only the record: hiding a broken object's mesh is the caller's.
    inline void LoadTarget(Damage::Target& t, const nlohmann::json& j)
    {
        t.health = j.value("health", t.health);
        t.destroyed = j.value("destroyed", t.destroyed);
        // Still counting down when saved: it stands until the fuse runs out.
        t.fuse = j.value("fuse", 0);
        t.state = t.destroyed ? Damage::STATE_DYING : Damage::STATE_NORMAL;
        t.hitCooldown = 0;
    }

    // ---- creatures --------------------------------------------------------

    inline nlohmann::json SaveEnemy(const Enemies::Enemy& e)
    {
        return {
            { "active", e.active }, { "alive", e.alive },
            { "x", e.x }, { "y", e.y }, { "z", e.z }, { "facing", e.facing },
            { "health", e.health }, { "state", e.state },
            { "drop", e.drop }, { "triggerCount", e.triggerCount },
            { "heldMonster", e.heldMonster }, { "deathChain", e.deathChain },
            { "alerted", e.ai.alerted },
            { "egg", { e.egg.state, e.egg.phase, e.egg.idleTimer, e.egg.hitTimer } },
            { "vent", { e.vent.state, e.vent.cycle, e.vent.emitCounter, e.vent.switchedOff } },
            { "colonist", { e.colonist.state, e.colonist.lastView, e.colonist.idleTimer } },
            { "animation", e.animation }, { "animationFrame", e.animationFrame },
            { "animationEnded", e.animationEnded }, { "bodyRemoved", e.bodyRemoved },
            { "type", e.type }, { "onCeiling", e.onCeiling },
        };
    }

    // Over a creature as the level built it. Whatever it was in the middle of
    // - a leap, a flinch, a face hug, an attack - is dropped: it comes back
    // standing, idle, and decides again. A creature dying when saved is dead.
    inline void LoadEnemy(Enemies::Enemy& e, const nlohmann::json& j)
    {
        e.active = j.value("active", e.active);
        e.alive = j.value("alive", e.alive);
        e.x = e.lastX = j.value("x", e.x);
        e.y = j.value("y", e.y);
        e.z = e.lastZ = j.value("z", e.z);
        e.facing = j.value("facing", e.facing) & PlayerCamera::ANGLE_MASK;
        e.health = j.value("health", e.health);
        e.drop = j.value("drop", e.drop);
        e.triggerCount = j.value("triggerCount", e.triggerCount);
        e.heldMonster = j.value("heldMonster", e.heldMonster);
        e.deathChain = j.value("deathChain", e.deathChain);
        e.bodyRemoved = j.value("bodyRemoved", false);
        // A ceiling creature that has let go is the warrior or dog it became.
        e.type = j.value("type", e.type);
        e.ai.type = e.type;
        e.onCeiling = j.value("onCeiling", e.onCeiling);
        e.ai.ceilingCrawler = e.onCeiling;

        const int state = j.value("state", e.state);
        if (!e.alive || state == EnemyBehaviour::STATE_DYING || state == EnemyBehaviour::STATE_DEAD)
        {
            e.alive = false;
            e.state = EnemyBehaviour::STATE_DEAD;
        }
        else
        {
            e.state = EnemyBehaviour::STATE_ACTING;
        }

        e.ai = CreatureAI::Agent{};
        e.ai.facing = e.ai.target = e.facing;
        e.ai.alerted = j.value("alerted", false);
        e.ai.state = CreatureAI::IDLE;
        e.ai.idleTimer = 0;

        if (j.contains("egg") && j["egg"].is_array() && j["egg"].size() == 4)
        {
            const auto& g = j["egg"];
            e.egg.state = g[0]; e.egg.phase = g[1]; e.egg.idleTimer = g[2]; e.egg.hitTimer = g[3];
        }
        if (j.contains("vent") && j["vent"].is_array() && j["vent"].size() == 4)
        {
            const auto& v = j["vent"];
            e.vent.state = v[0]; e.vent.cycle = v[1]; e.vent.emitCounter = v[2]; e.vent.switchedOff = v[3];
        }
        if (j.contains("colonist") && j["colonist"].is_array() && j["colonist"].size() == 3)
        {
            const auto& c = j["colonist"];
            e.colonist.state = c[0]; e.colonist.lastView = c[1]; e.colonist.idleTimer = c[2];
        }

        // Moment-to-moment state starts clear.
        e.hitCooldown = 0;
        e.pushX = e.pushZ = 0.0f;
        e.pendingDamage = 0;
        e.stepX = e.stepZ = 0.0f;
        e.vy = 0.0f;
        e.leaping = e.pouncing = e.springing = false;
        e.leapX = e.leapZ = 0.0f;
        e.pounceTicks = 0;
        e.actTicks = e.shotCountdown = 0;
        e.actAnimation = -1;
        e.grenadeArmed = e.fireGrenade = e.fireShot = false;
        e.recoveredFromHit = e.overlayFinished = false;

        // The animation: a scripted creature's is replayed up to where it
        // was (its script has no saved position); the others' frame is put
        // back as it was. Either keeps an opened egg open and a body down.
        const int animation = j.value("animation", -1);
        if (Enemies::Scripted(e))
        {
            // Its script from the start, stepped on to the frame it was on (and
            // to its end, if it had ended) - which keeps an opened egg open.
            namespace SA = ALTEngine::Formats::SpriteAnim;
            e.animation = -1;
            Enemies::PlayAnimation(e, animation);
            const int frame = j.value("animationFrame", 0);
            const bool ended = j.value("animationEnded", false);
            for (int step = 0; step < 4000 && !e.sequence.empty(); ++step)
            {
                if (e.animator.frameIndex == frame && (!ended || e.animator.Ended())) { break; }
                if (e.animator.Ended()) { break; }
                SA::Tick(e.animator, e.sequence);
            }
            e.animationFrame = e.animator.frameIndex;
            e.animationEnded = Enemies::ScriptEventFor(e);
            e.animSound = -1;
        }
        else
        {
            e.animation = animation;
            e.animationFrame = j.value("animationFrame", 0);
            e.animationTimer = 0;
            e.animationEnded = j.value("animationEnded", false);
        }
    }

    // ---- the player -------------------------------------------------------

    inline nlohmann::json SaveHud(const PlayerHudState& h)
    {
        return { { "health", h.health }, { "currentWeapon", h.currentWeapon }, { "grenades", h.grenades },
                 { "armourItem", h.armourItem }, { "terrainSpeed", h.terrainSpeed },
                 { "terrainShield", h.terrainShield } };
    }

    inline void LoadHud(PlayerHudState& h, const nlohmann::json& j)
    {
        h.health = j.value("health", h.health);
        h.currentWeapon = j.value("currentWeapon", h.currentWeapon);
        h.grenades = j.value("grenades", h.grenades);
        h.armourItem = j.value("armourItem", h.armourItem);
        h.terrainSpeed = j.value("terrainSpeed", h.terrainSpeed);
        h.terrainShield = j.value("terrainShield", h.terrainShield);
        h.damageCooldown = 0;
        h.dead = false;
    }

    // The player's facing and look; their speed starts at a standstill.
    inline nlohmann::json SaveCamera(const PlayerCamera::State& c)
    {
        return { { "yaw", c.yaw }, { "pitch", c.pitch } };
    }

    inline void LoadCamera(PlayerCamera::State& c, const nlohmann::json& j)
    {
        c = PlayerCamera::State{};
        c.yaw = j.value("yaw", 0) & PlayerCamera::ANGLE_MASK;
        c.pitch = j.value("pitch", 0);
        c.viewYaw = c.yaw;
        c.moveAngle = c.yaw;
    }
}
