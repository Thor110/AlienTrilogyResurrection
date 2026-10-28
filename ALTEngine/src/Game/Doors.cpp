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
            ds.progress = startsOpen ? travel : 0;
            ds.threshold = door.lockState == 0 ? 1 : door.lockState;
            ds.keepsUnlockOnClose = startsOpen;
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

    void MarkSingleUseDoors(const Formats::LevelGeometry& level, std::vector<DoorState>& doors)
    {
        std::vector<bool> reusable(doors.size(), false);
        for (const auto& trigger : level.actions)
        {
            if (trigger.enable != 0xFF) { continue; }
            int step = trigger.commandStart;
            for (int guard = 0; guard < 64 && step != 0xFF && static_cast<size_t>(step) < level.logics.size(); ++guard)
            {
                const auto& c = level.logics[static_cast<size_t>(step)];
                if ((c.action == 1 || c.action == 6) && c.objectIndex < doors.size()) { reusable[c.objectIndex] = true; }
                step = c.nextStep;
            }
        }
        for (size_t i = 0; i < doors.size(); ++i) { doors[i].staysOpen = !reusable[i]; }
    }

    bool ApplyCommand(DoorState& door, bool isOpenCommand, int amount, bool viaSwitch)
    {
        if (door.phase != DoorState::Phase::Idle || door.unlockProgress >= door.threshold) { return false; }
        if (isOpenCommand && door.unlockProgress < door.threshold - 1) { return false; }

        if (viaSwitch) { door.switchOperated = true; }
        door.unlockProgress = static_cast<uint8_t>(door.unlockProgress + amount);
        return true;
    }

    DoorNotice Tick(DoorState& door, Formats::LevelGeometry& level, float playerX, float playerZ)
    {
        DoorNotice notice = DoorNotice::None;

        switch (door.phase)
        {
        case DoorState::Phase::Idle:
            // Re-checked every tick against the counter alone - closing resets
            // the counter, so no fresh trigger is needed here.
            if (door.cooldown > 0) { door.cooldown--; }
            else if (door.unlockProgress >= door.threshold)
            {
                door.phase = DoorState::Phase::Opening;
                notice = door.isLift ? DoorNotice::LiftActivated
                       : door.switchOperated ? DoorNotice::DoorPoweredUp
                       : DoorNotice::DoorActivated;
            }
            break;

        case DoorState::Phase::Opening:
            door.progress++;
            if (door.progress >= door.travel) { door.progress = door.travel; door.phase = DoorState::Phase::Open; }
            break;

        case DoorState::Phase::Open:
        {
            if (door.switchOperated || door.staysOpen) { break; }
            // Measured across the doorway: a door running along Z is walked
            // through in X, so that is the axis that matters.
            float d = door.alongZ ? (door.worldX - playerX) : (door.worldZ - playerZ);
            if (d < 0) { d = -d; }
            bool occupied = false;
            for (int i = 0; i < door.cellCount; ++i)
            {
                if (level.collisionGrid[door.cells[static_cast<size_t>(i)]].unknown13 != 0) { occupied = true; break; }
            }
            if (d > 1024.0f && !occupied) { door.phase = DoorState::Phase::Closing; }
            break;
        }

        case DoorState::Phase::Closing:
            door.progress--;
            if (door.progress <= 0)
            {
                door.progress = 0;
                door.phase = DoorState::Phase::Idle;
                if (door.keepsUnlockOnClose) { door.cooldown = door.holdTicks; }
                else { door.unlockProgress = 0; }
            }
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
