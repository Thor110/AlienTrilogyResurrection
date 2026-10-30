#include "Doors.h"

namespace ALTEngine::Game::Doors
{
    int TravelForLevel(const std::string& levelDigits)
    {
        return (levelDigits == "111" || levelDigits == "112") ? 30 : 43;
    }

    std::vector<DoorState> Build(Formats::LevelGeometry& level, int travel, float originX, float originZ)
    {
        std::vector<DoorState> doors;
        doors.reserve(level.doors.size());

        for (const auto& door : level.doors)
        {
            // Orientation 2 and 6 run along Z, 0 and 4 along X. 0 vs 4 and 2
            // vs 6 are 180 degrees apart and only affect the model's yaw.
            const bool alongZ = (door.rotation == 2 || door.rotation == 6);

            // The mesh origin is the centre of the 4-cell span, which runs from
            // grid-1 to grid+2 - so it lands on the boundary between grid and
            // grid+1 along the door's axis, and on a cell centre across it.
            const float gameX = static_cast<float>(door.x) * 512.0f + (alongZ ? 256.0f : 512.0f);
            const float gameZ = static_cast<float>(door.y) * 512.0f + (alongZ ? 512.0f : 256.0f);

            // Four covered cells, anchored one cell back along the door's axis.
            const int anchorX = static_cast<int>(door.x) - (alongZ ? 0 : 1);
            const int anchorZ = static_cast<int>(door.y) - (alongZ ? 1 : 0);

            const size_t ownCell = static_cast<size_t>(door.y) * level.header.mapLength + static_cast<size_t>(door.x);
            const int cellFloor = (ownCell < level.collisionGrid.size()) ? level.collisionGrid[ownCell].floorHeight : 0;

            // Flag 0x40: the door starts open, its ceiling already raised by
            // the full travel and the mesh at that height.
            const bool startsOpen = (door.unknown & 0x40) != 0;
            const int ceilingUnits = startsOpen ? cellFloor + travel : cellFloor;

            DoorState ds;
            ds.alongZ = alongZ;
            ds.floorUnits = cellFloor;
            ds.travel = travel;
            ds.counter = startsOpen ? travel : 0;
            ds.progress = startsOpen ? travel : 0;
            ds.threshold = door.lockState == 0 ? 1 : door.lockState;
            ds.startsOpen = startsOpen;
            ds.holdTicks = static_cast<int>(door.time) * 4;
            ds.phase = startsOpen ? DoorState::Phase::Open : DoorState::Phase::Idle;
            ds.worldX = gameX - originX;
            ds.worldZ = gameZ - originZ;
            ds.baseY = static_cast<float>(cellFloor) * 32.0f + 16.0f;
            ds.startY = static_cast<float>(startsOpen ? ceilingUnits : cellFloor) * 32.0f + 16.0f;

            for (int step = 0; step < 4; ++step)
            {
                const int cx = anchorX + (alongZ ? 0 : step);
                const int cz = anchorZ + (alongZ ? step : 0);
                if (auto* cell = Formats::CellAt(level, cx, cz))
                {
                    cell->floorHeight = static_cast<uint8_t>(cellFloor);
                    cell->ceilingHeight = static_cast<uint8_t>(ceilingUnits);
                }
                const int ci = Formats::CellIndex(level, cx, cz);
                if (ci >= 0) { ds.cells[static_cast<size_t>(ds.cellCount++)] = static_cast<size_t>(ci); }
            }
            doors.push_back(ds);
        }
        return doors;
    }

    bool ApplyCommand(DoorState& door, bool isOpenCommand, int amount, bool viaSwitch)
    {
        // Flags 0x08/0x10 (opening, closing) refuse; a door that is shut, open
        // or holding takes the count.
        if (door.phase == DoorState::Phase::Opening || door.phase == DoorState::Phase::Closing) { return false; }
        if (door.unlockProgress >= door.threshold) { return false; }
        if (isOpenCommand && door.unlockProgress < door.threshold - 1) { return false; }

        door.switchOperated = viaSwitch;
        door.unlockProgress = static_cast<uint8_t>(door.unlockProgress + amount);
        return true;
    }

    namespace
    {
        // FUN_00028d90: the doorway is blocked if anything stands in the
        // door's two middle cells, or in the cell either side of each across
        // the doorway (x +/- 1 for a door along Z, a row either way for one
        // along X). Byte 0x0c of a cell is its occupier.
        bool Occupied(const DoorState& door, const Formats::LevelGeometry& level)
        {
            if (door.cellCount < 3) { return false; }
            const long long across = door.alongZ ? 1 : static_cast<long long>(level.header.mapLength);
            const long long size = static_cast<long long>(level.collisionGrid.size());
            for (int middle = 1; middle <= 2; ++middle)
            {
                const long long c = static_cast<long long>(door.cells[static_cast<size_t>(middle)]);
                for (long long cell : { c, c - across, c + across })
                {
                    if (cell >= 0 && cell < size && level.collisionGrid[static_cast<size_t>(cell)].unknown13 != 0) { return true; }
                }
            }
            return false;
        }

        // The end of a close (0x2932a) or of an open (0x294a6): a door with no
        // open time, and a door that does not start open, is re-locked and
        // shut; any other waits out its hold first.
        void FinishClose(DoorState& door)
        {
            door.isOpen = false;
            door.holdCounter = door.holdTicks;
            if (door.holdTicks == 0 || !door.startsOpen) { door.unlockProgress = 0; door.phase = DoorState::Phase::Idle; }
            else { door.phase = DoorState::Phase::Hold; }
        }

        void FinishOpen(DoorState& door)
        {
            door.isOpen = true;
            door.holdCounter = door.holdTicks;
            if (door.holdTicks == 0 || door.startsOpen) { door.unlockProgress = 0; door.phase = DoorState::Phase::Open; }
            else { door.phase = DoorState::Phase::Hold; }
        }
    }

    DoorNotice Tick(DoorState& door, Formats::LevelGeometry& level, float playerX, float playerZ)
    {
        DoorNotice notice = DoorNotice::None;

        switch (door.phase)
        {
        case DoorState::Phase::Idle:
            // Re-checked every tick against the count alone.
            if (door.unlockProgress >= door.threshold)
            {
                door.phase = DoorState::Phase::Opening;
                notice = door.isLift ? DoorNotice::LiftActivated
                       : door.switchOperated ? DoorNotice::DoorPoweredUp
                       : DoorNotice::DoorActivated;
            }
            break;

        case DoorState::Phase::Opening:
            // The step that reaches the full count does not raise the door, so
            // a door opened from shut stops one unit short of its travel.
            if (++door.counter >= door.travel) { door.counter = door.travel; FinishOpen(door); }
            else { door.progress++; }
            break;

        case DoorState::Phase::Hold:
            if (--door.holdCounter <= 0)
            {
                door.holdCounter = 0;
                door.phase = door.isOpen ? DoorState::Phase::Open : DoorState::Phase::Idle;
            }
            break;

        case DoorState::Phase::Open:
        {
            if (door.unlockProgress < door.threshold) { break; }
            // Measured across the doorway: a door running along Z is walked
            // through in X, so that is the axis that matters.
            float d = door.alongZ ? (door.worldX - playerX) : (door.worldZ - playerZ);
            if (d < 0) { d = -d; }
            if (d > 1024.0f && !Occupied(door, level)) { door.phase = DoorState::Phase::Closing; }
            break;
        }

        case DoorState::Phase::Closing:
            --door.counter;
            if (Occupied(door, level)) { door.phase = DoorState::Phase::Opening; }
            else if (door.counter <= 0) { door.counter = 0; FinishClose(door); }
            else if (door.progress > 0) { door.progress--; }
            break;
        }

        const uint8_t ceilingUnits = static_cast<uint8_t>(door.floorUnits + door.progress);
        for (int i = 0; i < door.cellCount; ++i)
        {
            level.collisionGrid[door.cells[static_cast<size_t>(i)]].ceilingHeight = ceilingUnits;
        }
        return notice;
    }
}
