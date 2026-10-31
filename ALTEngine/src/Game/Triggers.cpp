#include "Triggers.h"

namespace ALTEngine::Game::Triggers
{
    System::System(const Formats::LevelGeometry& level)
        : level_(&level)
    {
        uses_.reserve(level.actions.size());
        for (const auto& trigger : level.actions) { uses_.push_back(trigger.enable); }
    }

    int System::CellTriggerId(const Formats::LevelGeometry& level, int cellIndex)
    {
        if (cellIndex < 0 || static_cast<size_t>(cellIndex) >= level.collisionGrid.size()) { return 0; }
        return level.collisionGrid[static_cast<size_t>(cellIndex)].scriptAction;
    }

    int System::UsesRemaining(int triggerId) const
    {
        if (triggerId < 0 || static_cast<size_t>(triggerId) >= uses_.size()) { return 0; }
        return uses_[static_cast<size_t>(triggerId)];
    }

    bool System::Run(int triggerId, Source source, const Handlers& handlers)
    {
        if (!level_ || triggerId <= 0 || static_cast<size_t>(triggerId) >= level_->actions.size()) { return false; }
        const auto& trigger = level_->actions[static_cast<size_t>(triggerId)];
        uint8_t& uses = uses_[static_cast<size_t>(triggerId)];
        const uint8_t needed = (source == Source::Shot) ? FLAG_SHOT
                             : (source == Source::EnemyThrow) ? FLAG_ENEMY_THROW
                             : (source == Source::Creature) ? FLAG_CREATURE : FLAG_PLAYER;
        if ((trigger.activationMask & needed) == 0 || uses == 0) { return false; }
        if (!RunChain(trigger.commandStart, source, handlers)) { return false; }
        if (uses != USES_UNLIMITED) { uses--; }
        return true;
    }

    bool System::RunCommands(int commandStart, const Handlers& handlers)
    {
        if (!level_) { return false; }
        return RunChain(commandStart, Source::WalkOn, handlers);
    }

    bool System::RunChain(int commandStart, Source source, const Handlers& handlers)
    {
        // The chain's result, as the original keeps it in ECX across the loop
        // (0x415a2): it starts as a failure; a command that runs sets it (an
        // effect to success, a door or lift to its own answer, the level's
        // end to success); a command the runner has no arm for leaves it as
        // it was. The loop stops as soon as it reads failure. So a pass-over
        // command first in a chain fails it, and after a success it is simply
        // stepped past.
        const auto& commands = level_->logics;
        int step = commandStart;
        bool sawActivateObject = false;
        bool result = false;
        const bool notPlayer = source == Source::Shot || source == Source::EnemyThrow || source == Source::Creature;

        for (int guard = 0; step != CHAIN_END; ++guard)
        {
            // A malformed chain - out of range, or looping - is treated as a
            // failure rather than run forever or partly.
            if (guard >= MAX_CHAIN_LENGTH || step < 0 || static_cast<size_t>(step) >= commands.size()) { return false; }
            const auto& c = commands[static_cast<size_t>(step)];
            const int amount = c.modifier;
            const int signedAmount = static_cast<int8_t>(c.modifier);
            const int target = c.objectIndex;

            switch (c.action)
            {
            case TOGGLE_LIGHT:
                if (handlers.toggleLight) { handlers.toggleLight(target, signedAmount); }
                result = true;
                break;
            case CHANGE_TEXTURE:
                if (handlers.changeTexture) { handlers.changeTexture(target, signedAmount); }
                result = true;
                break;
            case OPEN_DOOR:
                // Only the player's own runner has an arm for a door's final
                // opening (FUN_00040ff0, with the use key): a round
                // (FUN_000414f0), a grenade (FUN_00041784) and a creature
                // (FUN_0004129c) pass over it - their jump tables send 4 and 6
                // to the loop's end (0x41706).
                if (notPlayer) { break; }
                result = handlers.door && handlers.door(target, true, amount, sawActivateObject, source);
                break;
            case UNLOCK_DOOR:
                result = handlers.door && handlers.door(target, false, amount, sawActivateObject, source);
                break;
            case SPAWN_MONSTER:
                // FUN_0002f224 adds to the monster's counter whether or not
                // that releases it, and the runner counts it a success.
                if (handlers.spawnMonster) { handlers.spawnMonster(target, amount); }
                result = true;
                break;
            case ACTIVATE_OBJECT:
                // No arm in a round's, a grenade's or a creature's runner.
                if (notPlayer) { break; }
                result = source == Source::Interact && handlers.activateObject && handlers.activateObject(target);
                if (result) { sawActivateObject = true; }
                break;
            case SPAWN_PICKUP:
                if (handlers.spawnPickup) { handlers.spawnPickup(target, signedAmount); }
                result = true;
                break;
            case LIFT_FINAL:
            case LIFT_ADD:
                // Conditional, like the doors: a lift that is moving or already
                // due to go refuses, and that fails the chain.
                result = handlers.lift && handlers.lift(target, amount, c.action == LIFT_FINAL);
                break;
            case END_LEVEL:
                if (handlers.endLevel) { handlers.endLevel(); }
                result = true;
                break;
            default:
                // An unused slot (0xFF, which L111's trigger 7 runs into after
                // its last command) or anything else past 9: every runner's
                // range check sends it to the loop's end, a pass-over.
                break;
            }
            if (!result) { return false; }
            step = c.nextStep;
        }
        return result;
    }

    bool System::OnShotEnterCell(int& roundLatch, int cellIndex, const Handlers& handlers, Source source)
    {
        if (!level_) { return false; }
        const int id = CellTriggerId(*level_, cellIndex);
        if (id == 0) { roundLatch = 0; return false; }
        if (id == roundLatch) { return false; }
        if (Run(id, source, handlers)) { roundLatch = id; return true; }
        roundLatch = 0;
        return false;
    }

    bool System::OnCreatureEnterCell(int& latch, int cellIndex, const Handlers& handlers)
    {
        if (!level_) { return false; }
        const int id = CellTriggerId(*level_, cellIndex);
        if (id == 0) { latch = 0; return false; }
        if (id == latch) { return false; }
        if (Run(id, Source::Creature, handlers)) { latch = id; return true; }
        // Only a trigger that answers to creatures, with uses left, touches
        // the latch (FUN_0004129c's outer test).
        const auto& trigger = level_->actions[static_cast<size_t>(id)];
        if ((trigger.activationMask & FLAG_CREATURE) != 0 && UsesRemaining(id) != 0) { latch = 0; }
        return false;
    }

    bool System::OnEnterCell(int& lastId, int cellIndex, const Handlers& handlers)
    {
        if (!level_) { return false; }
        const int id = CellTriggerId(*level_, cellIndex);
        const bool fire = id != 0 && id != lastId;
        lastId = id;
        return fire && Run(id, Source::WalkOn, handlers);
    }
}
