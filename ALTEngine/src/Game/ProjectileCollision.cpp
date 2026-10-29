#include "ProjectileCollision.h"

#include <cmath>

namespace ALTEngine::Game::ProjectileCollision
{
    CellResult TestCell(const Formats::LevelGeometry& level, int gameX, int gameZ, float y,
                        int projectileType, int strength)
    {
        CellResult result;
        const auto* cell = Formats::CellAt(level, gameX >> 9, gameZ >> 9);
        if (!cell) { result.blocked = true; return result; }   // off the map: the task frees it

        auto hitObject = [&](int damage) {
            result.objectHit = cell->unknown5;
            result.damage = damage;
        };

        if (cell->attribute == 0x14) { result.blocked = true; return result; }

        const uint8_t occupant = cell->unknown13;
        if (occupant == 0x15)
        {
            if (projectileType == TYPE_SMARTGUN) { hitObject(strength); }
            result.blocked = true;
            return result;
        }
        if (occupant == 0x1d)
        {
            if (projectileType <= TYPE_PULSE || projectileType == TYPE_SMARTGUN) { hitObject(strength); }
            result.blocked = true;
            return result;
        }
        if (occupant == 0x1c)
        {
            switch (projectileType)
            {
            case TYPE_PISTOL: case TYPE_SHOTGUN: case TYPE_PULSE: case TYPE_SMARTGUN: hitObject(strength); break;
            case TYPE_GRENADE: hitObject(strength << 4); break;
            default: break;
            }
            result.blocked = true;
            return result;
        }

        const float floor = Formats::FindFloorHeightGridSpace(level, gameX, gameZ);
        if (occupant > 0x13 && occupant != 0xFF)
        {
            const float window = (occupant == 0x19) ? 0x400 : 0x200;
            if (floor <= y && y <= floor + window)
            {
                if (!(projectileType == TYPE_PISTOL && occupant == 0x17)) { hitObject(strength); }
                result.blocked = true;
                return result;
            }
        }

        if (cell->attribute == 9) { if (y < floor + 0x180) { result.blocked = true; return result; } }
        else if (cell->attribute == 8) { if (y < floor + 0x100) { result.blocked = true; return result; } }

        if (y < floor) { result.blocked = true; return result; }
        const float ceiling = static_cast<float>(cell->ceilingHeight) * Formats::WORLD_UNITS_PER_HEIGHT_UNIT;
        result.blocked = y > ceiling;
        return result;
    }

    HalfExtents CreatureExtents(int creatureType)
    {
        switch (creatureType)
        {
        case 1: case 2: case 3: case 4: case 10:
            return { 0x100, 0x200, 0x100 };
        case 5: case 6: case 11: case 12: case 13: case 14:
        case 0x10: case 0x11: case 0x12: case 0x13:
            return { 0x100, 0x300, 0x100 };
        case 7:
            return { 0x180, 0x600, 0x180 };
        case 8: case 9:
            return { 0x100, 0x100, 0x100 };
        default:
            // Types FUN_0002e638 has no arm for keep whatever the entity slot
            // last held. The commonest size stands in.
            return {};
        }
    }

    bool OverlapsCreature(float px, float py, float pz, float cx, float cy, float cz,
                          int creatureType, int projectileType)
    {
        HalfExtents e = CreatureExtents(creatureType);
        if (projectileType == TYPE_SHOTGUN) { e.x *= 2; e.y *= 2; e.z *= 2; }
        return std::fabs(px - cx) < static_cast<float>(e.x)
            && std::fabs(py - cy) < static_cast<float>(e.y)
            && std::fabs(pz - cz) < static_cast<float>(e.z);
    }

    uint16_t CellZones(const Formats::LevelGeometry& level, int cellIndex)
    {
        if (cellIndex < 0 || static_cast<size_t>(cellIndex) >= level.collisionGrid.size()) { return 0; }
        const auto& cell = level.collisionGrid[static_cast<size_t>(cellIndex)];
        return static_cast<uint16_t>(cell.ceilingFog | (cell.floorFog << 8));
    }

    bool ShareZone(const Formats::LevelGeometry& level, int cellA, int cellB)
    {
        return (CellZones(level, cellA) & CellZones(level, cellB)) != 0;
    }

    int BloodTypeForCreature(int creatureType)
    {
        switch (creatureType)
        {
        case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
            return 9;
        case 10: case 11: case 12: case 14:
            return 8;
        case 13:
            return 10;
        default:
            return -1;
        }
    }

    int ObjectHealth(const Formats::Crate& record)
    {
        return static_cast<int>(record.healthDiv16) << 4;
    }

    bool ObjectIndestructible(const Formats::Crate& record)
    {
        if ((record.flags & 1) != 0) { return true; }
        switch (record.type)
        {
        case 0x16: case 0x18: case 0x1a: case 0x1b:
        case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25:
            return true;
        default:
            return false;
        }
    }

    bool AcidTouchesPlayer(float dropX, float dropY, float dropZ, float playerX, float playerFeetY, float playerZ)
    {
        const float above = dropY - playerFeetY;
        if (above < 0.0f || above > 0x400) { return false; }
        return std::fabs(dropX - playerX) < 0x100 && std::fabs(dropZ - playerZ) < 0x100;
    }
}
