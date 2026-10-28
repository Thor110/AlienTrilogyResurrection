#pragma once

#include "../Formats/LevelLoader.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace ALTEngine::Game
{
    // Cell triggers: the one place a level's scripted behaviour runs.
    //
    // DATA LAYOUT (checked against the demo disc's L111/L112 and ALTViewer):
    //   - A collision cell's byte 15 IS the trigger id - an index straight into
    //     the 64-entry trigger table. 0 means no trigger. The ids in use run
    //     from 1 to the low 40s, never past 64, so there is no stride to divide
    //     out.
    //   - A trigger record is { flags, first command, uses, 0 }. Every trigger
    //     the demo levels use has flag 0x01 set; that is the "player can set
    //     this off" bit tested here.
    //   - `uses` is how many more times the trigger may run. 0xFF is unlimited;
    //     anything else is decremented each time the chain completes and the
    //     trigger goes dead at zero.
    //   - A command is { action, next command, amount, target }. The chain ends
    //     at next == 0xFF.
    //
    // CHAINS ARE CONDITIONAL. A command that fails aborts the rest of the chain
    // and the trigger is not used up. That is how a level says "open the door
    // only if the switch was actually thrown".
    //
    // The game previously had two separate implementations of this, which
    // disagreed on the id (one divided by four), the enable bit (one required
    // 0x04, which no trigger in the data has) and the action numbering - so
    // monster spawn triggers could never fire.
    namespace Triggers
    {
        enum Action : uint8_t
        {
            TOGGLE_LIGHT = 0,     // light record, signed delta
            UNLOCK_DOOR = 1,      // door, amount added to its unlock counter
            SPAWN_PICKUP = 2,     // pickup record, amount added to its trigger counter (FUN_0003bb14)
            SPAWN_MONSTER = 3,    // monster record, amount added to its trigger counter
            ACTIVATE_OBJECT = 4,  // switch object; interact only, latches
            LIFT_FINAL = 5,       // lift, amount added only as the final increment (FUN_00041e20)
            OPEN_DOOR = 6,        // door, as UNLOCK_DOOR but must be the final increment
            LIFT_ADD = 7,         // lift, amount added to its counter (FUN_00041dd8)
            END_LEVEL = 8,
            CHANGE_TEXTURE = 9,   // texture animator, signed delta
            EMPTY_SLOT = 0xFF,    // unused command slot - ends the chain
        };

        inline constexpr uint8_t FLAG_PLAYER = 0x01;
        // Set off by a ROUND entering the cell (FUN_000414f0, run by the particle
        // task for every live projectile). L111's trigger 12 has both bits.
        inline constexpr uint8_t FLAG_SHOT = 0x02;
        inline constexpr uint8_t USES_UNLIMITED = 0xFF;
        inline constexpr uint8_t CHAIN_END = 0xFF;
        inline constexpr int MAX_CHAIN_LENGTH = 64;

        // How the player set the trigger off. Walking onto the cell is what the
        // original's mover does; Interact is the use key, which is the only way
        // a switch object (ACTIVATE_OBJECT) can be thrown.
        enum class Source { WalkOn, Interact, Shot };

        // The world side of each command. A handler returning false is a failed
        // command and aborts the chain. Unset handlers are treated as success
        // for the effect-only commands and failure for the conditional ones.
        struct Handlers
        {
            // doorIndex, isOpenCommand, amount, viaSwitch (an ACTIVATE_OBJECT
            // succeeded earlier in this chain), source
            std::function<bool(int, bool, int, bool, Source)> door;
            std::function<void(int, int)> toggleLight;      // light, signed delta
            std::function<void(int, int)> changeTexture;    // animator, signed delta
            std::function<void(int, int)> spawnMonster;     // monster, amount
            std::function<void(int, int)> spawnPickup;      // pickup, amount
            std::function<bool(int)> activateObject;        // object record
            // lift record, amount, final-increment-only; false fails the chain
            std::function<bool(int, int, bool)> lift;
            std::function<void()> endLevel;
        };

        class System
        {
        public:
            System() = default;
            explicit System(const Formats::LevelGeometry& level);

            // The trigger id on a cell, or 0.
            static int CellTriggerId(const Formats::LevelGeometry& level, int cellIndex);

            // Runs one trigger's chain. Returns true if the chain ran to its end
            // (and so used the trigger up once).
            bool Run(int triggerId, Source source, const Handlers& handlers);

            // Cell entry: fires the cell's trigger once on entering it, and not
            // again while the entity stays on cells with the same id. `lastId`
            // is the entity's own latch (the original keeps it at entity+0x71).
            // Returns true if a chain completed.
            bool OnEnterCell(int& lastId, int cellIndex, const Handlers& handlers);

            // The same for a round in flight, with the round's own latch
            // (particle +0x1f). FUN_000414f0 differs from the walk-on path in
            // one way: a chain that fails clears the latch, so the round tries
            // again next tick while it is still in the cell.
            bool OnShotEnterCell(int& roundLatch, int cellIndex, const Handlers& handlers);

            int UsesRemaining(int triggerId) const;

            // FUN_00040e2c: a command chain run straight from its first
            // command, with no trigger in front of it - no activation mask and
            // no uses. A dying queen runs the one its record's byte 6 names.
            bool RunCommands(int commandStart, const Handlers& handlers);

        private:
            bool RunChain(int commandStart, Source source, const Handlers& handlers);

            const Formats::LevelGeometry* level_ = nullptr;
            std::vector<uint8_t> uses_;
        };
    }
}
