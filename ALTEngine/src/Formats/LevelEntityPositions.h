#pragma once

#include "LevelLoader.h"

namespace ALTEngine::Formats
{
    struct WorldPosition
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    // Computes a world-space position for a level entity, matching
    // Edward's own ObjectSpawner.cs exactly: raw grid X/Y used directly
    // (X negated, e.g. "new Vector3(-crate.X, ..., crate.Y)"), no scale
    // factor applied - confirmed identical across every entity type in
    // that file (collision nodes, crates, monsters, pickups, lifts,
    // doors all follow this same pattern.
    //
    // The one thing this can't reproduce exactly: ObjectSpawner.cs finds
    // the actual floor height via Physics.Raycast against the real mesh
    // at runtime, which needs a full 3D collision system this engine
    // doesn't have yet. As a practical substitute, this looks up
    // floorHeight from the already-parsed collision grid cell at
    // (gridX, gridY) instead - the same data ObjectSpawner.cs's
    // CollisionNode carries, just read directly rather than derived by
    // raycasting against geometry. Entities that start at Y=-10 in the
    // reference (Crate/Door/Lift, deliberately below-floor so the
    // upward raycast is guaranteed to hit something) don't need that
    // trick here since there's no raycast to guarantee a hit for -
    // floorHeight is used directly.
    //
    // The scale question (raw grid values, e.g. 0-255, used directly as
    // world units, on the same numeric scale as vertex positions
    // spanning +/-27000) remains open - this follows Edward's own code
    // exactly rather than guessing at a correction, per his own
    // "technically cell size shouldn't matter so long as it's
    // consistent" guidance.
    inline WorldPosition ComputeEntityWorldPosition(int gridX, int gridY, const LevelGeometry& level)
    {
        WorldPosition pos;
        pos.x = static_cast<float>(-gridX);
        pos.z = static_cast<float>(gridY);

        size_t cellIndex = static_cast<size_t>(gridY) * level.header.mapWidth + static_cast<size_t>(gridX);
        if (cellIndex < level.collisionGrid.size())
        {
            pos.y = static_cast<float>(level.collisionGrid[cellIndex].floorHeight);
        }
        return pos;
    }
}
