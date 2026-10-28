#include "Lifts.h"

namespace ALTEngine::Game::Lifts
{
    namespace
    {
        // FUN_00041e80: every cell of the lift gets this floor byte.
        void SetFloor(const Lift& lift, Formats::LevelGeometry& level, int height)
        {
            for (int dz = 0; dz < lift.depth; ++dz)
            {
                for (int dx = 0; dx < lift.width; ++dx)
                {
                    const int index = Formats::CellIndex(level, lift.cellX + dx, lift.cellZ + dz);
                    if (index >= 0 && static_cast<size_t>(index) < level.collisionGrid.size())
                    {
                        level.collisionGrid[static_cast<size_t>(index)].floorHeight = static_cast<uint8_t>(height);
                    }
                }
            }
        }

        void SetOffDown(Lift& lift, TickResult& r)
        {
            lift.flags = static_cast<uint8_t>((lift.flags & 0xf6) | FALLING);
            lift.phase = Phase::Descend;
            r.started = true;
        }

        void SetOffUp(Lift& lift, TickResult& r)
        {
            lift.flags = static_cast<uint8_t>((lift.flags & 0xf9) | RISING);
            lift.phase = Phase::Ascend;
            r.started = true;
        }

        // What an arrival does next (the tail of 0x41fa0 / 0x420ec). Without
        // its own cycle, a lift with a dwell goes back to where it started
        // after the dwell, and is then idle there with its counter cleared.
        void Arrive(Lift& lift, bool atTop)
        {
            if (lift.flags & CYCLES) { lift.dwellCounter = lift.dwell; lift.phase = Phase::Wait; return; }
            if (lift.dwell == 0) { lift.counter = 0; lift.phase = Phase::Idle; return; }
            const bool backAtStart = atTop == ((lift.flags & STARTED_HIGH) != 0);
            if (backAtStart) { lift.counter = 0; lift.phase = Phase::Idle; return; }
            lift.dwellCounter = lift.dwell;
            lift.phase = Phase::Wait;
        }
    }

    std::vector<Lift> Build(Formats::LevelGeometry& level)
    {
        std::vector<Lift> lifts;
        lifts.reserve(level.lifts.size());
        for (const auto& record : level.lifts)
        {
            Lift lift;
            lift.cellX = record.x;
            lift.cellZ = record.y;
            lift.width = record.z;
            lift.depth = record.unknown1;
            // FUN_00041b30, run on the records as the level loads: the
            // floor byte of the lift's first cell becomes its low height and
            // is added to the high one, so the file's high byte is a travel
            // above the map's own floor (byte 4 is always 0 on disc).
            const auto* base = Formats::CellAt(level, record.x, record.y);
            const int baseFloor = base ? base->floorHeight : 0;
            lift.low = baseFloor;
            lift.high = (record.unknown3 + baseFloor) & 0xff;
            lift.flags = record.unknown5;
            lift.dwellCounter = record.unknown6;
            lift.dwell = record.unknown7;
            lift.threshold = record.unknown8;
            lift.counter = record.unknown9;
            lift.model = record.unknown10;
            if (lift.dwellCounter != 0) { lift.flags |= CYCLES; }
            if (lift.flags & AT_TOP)
            {
                lift.flags |= STARTED_HIGH;
                lift.height = lift.high;
            }
            else
            {
                lift.height = lift.low;
            }
            SetFloor(lift, level, lift.height);
            lifts.push_back(lift);
        }
        return lifts;
    }

    bool AddToCounter(Lift& lift, int amount, bool finalIncrementOnly)
    {
        if (lift.flags & (RISING | FALLING)) { return false; }
        if (lift.counter >= lift.threshold) { return false; }
        if (finalIncrementOnly && lift.counter < lift.threshold - 1) { return false; }
        lift.counter = (lift.counter + amount) & 0xff;
        return true;
    }

    TickResult Tick(Lift& lift, Formats::LevelGeometry& level)
    {
        TickResult r;
        const int step = (lift.flags & FAST) ? 2 : 1;
        switch (lift.phase)
        {
        case Phase::Idle:
            if (lift.counter < lift.threshold) { break; }
            if (!(lift.flags & RAN_ONCE))
            {
                lift.flags |= RAN_ONCE;
                r.firstActivation = true;
            }
            if (lift.flags & AT_TOP) { SetOffDown(lift, r); }
            else { SetOffUp(lift, r); }
            break;

        case Phase::Wait:
            if (--lift.dwellCounter != 0) { break; }
            if (lift.flags & AT_TOP) { SetOffDown(lift, r); }
            else { SetOffUp(lift, r); }
            break;

        case Phase::Descend:
            // The original stops on an exact match only, so a fast lift with an
            // odd travel would sail past its end; clamping is the one liberty.
            lift.height -= step;
            if (lift.height < lift.low) { lift.height = lift.low; }
            SetFloor(lift, level, lift.height);
            if (lift.height == lift.low)
            {
                lift.flags = static_cast<uint8_t>((lift.flags & 0xf3) | AT_BOTTOM);
                r.stopped = true;
                Arrive(lift, false);
            }
            break;

        case Phase::Ascend:
            lift.height += step;
            if (lift.height > lift.high) { lift.height = lift.high; }
            SetFloor(lift, level, lift.height);
            if (lift.height == lift.high)
            {
                lift.flags = static_cast<uint8_t>((lift.flags & 0xfc) | AT_TOP);
                r.stopped = true;
                Arrive(lift, true);
            }
            break;
        }
        return r;
    }
}
