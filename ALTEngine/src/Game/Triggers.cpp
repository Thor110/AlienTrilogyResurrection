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
        const uint8_t needed = (source == Source::Shot) ? FLAG_SHOT : FLAG_PLAYER;
        if ((trigger.activationMask & needed) == 0 || uses == 0) { return false; }

        const auto& commands = level_->logics;
        int step = trigger.commandStart;
        bool sawActivateObject = false;

        for (int guard = 0; step != CHAIN_END; ++guard)
        {
            // A malformed chain - out of range, or looping - is treated as a
            // failure rather than run forever or partly.
            if (guard >= MAX_CHAIN_LENGTH || step < 0 || static_cast<size_t>(step) >= commands.size()) { return false; }
            const auto& c = commands[static_cast<size_t>(step)];
            const int amount = c.modifier;
            const int signedAmount = static_cast<int8_t>(c.modifier);
            const int target = c.objectIndex;

            bool ok = true;
            switch (c.action)
            {
            case TOGGLE_LIGHT:
                if (handlers.toggleLight) { handlers.toggleLight(target, signedAmount); }
                break;
            case CHANGE_TEXTURE:
                if (handlers.changeTexture) { handlers.changeTexture(target, signedAmount); }
                break;
            case UNLOCK_DOOR:
            case OPEN_DOOR:
                ok = handlers.door && handlers.door(target, c.action == OPEN_DOOR, amount, sawActivateObject, source);
                break;
            case SPAWN_MONSTER:
                // Never fails the chain in the original: the spawn adds to the
                // monster's counter whether or not that releases it.
                if (handlers.spawnMonster) { handlers.spawnMonster(target, amount); }
                break;
            case ACTIVATE_OBJECT:
                // A round cannot throw a switch: FUN_000414f0 has no arm for
                // this command and simply moves on to the next.
                if (source == Source::Shot) { break; }
                ok = source == Source::Interact && handlers.activateObject && handlers.activateObject(target);
                if (ok) { sawActivateObject = true; }
                break;
            case SPAWN_PICKUP:
                // Like Spawn Monster, never fails the chain.
                if (handlers.spawnPickup) { handlers.spawnPickup(target, signedAmount); }
                break;
            case END_LEVEL:
                if (handlers.endLevel) { handlers.endLevel(); }
                break;
            case EMPTY_SLOT:
                // An unused command slot (all four bytes 0xFF). L111's trigger
                // 7 runs into one after its last real command, so this is the
                // end of the chain, not a failure.
                step = CHAIN_END;
                continue;
            default:
                // Lifts (5, 7) and anything unknown. Failing is the safe choice:
                // continuing would run later commands without their condition.
                ok = false;
                break;
            }
            if (!ok) { return false; }
            step = c.nextStep;
        }

        if (uses != USES_UNLIMITED) { uses--; }
        return true;
    }

    bool System::OnShotEnterCell(int& roundLatch, int cellIndex, const Handlers& handlers)
    {
        if (!level_) { return false; }
        const int id = CellTriggerId(*level_, cellIndex);
        if (id == 0) { roundLatch = 0; return false; }
        if (id == roundLatch) { return false; }
        if (Run(id, Source::Shot, handlers)) { roundLatch = id; return true; }
        roundLatch = 0;
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
