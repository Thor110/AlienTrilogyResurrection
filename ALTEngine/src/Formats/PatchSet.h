#pragma once

#include <string>
#include <vector>

#include "BinaryUtility.h"

namespace ALTEngine::Formats
{
    // One BinaryUtility.ReplaceBytes()/ReplaceByte() call from the
    // original ALTViewer.cs patch routine: a target file (relative to the
    // game root, e.g. "SECT90/L906LEV.MAP") and the edits to apply to it,
    // in order.
    struct PatchOperation
    {
        std::string targetFile;
        // Which stage it belongs to: "level" (geometry, objects, cells, lifts)
        // or "texture" (texture indices, turns, flips, draw routines, the GFX
        // sheets). Every level patch runs before every texture patch.
        std::string stage;
        std::string note;
        std::vector<BinaryEdit> edits;
    };
}
