#pragma once

#include "../Menu/MenuNode.h"
#include "../Bootstrap/Localization.h"

namespace ALTEngine::Screens
{
    // Builds the in-game pause menu's navigable tree: Auto Mapper /
    // Shoulder Lamp / 9mm Pistol / Shotgun / Flamethrower / Pulse Rifle /
    // Smart Gun / Batteries / Mission / Options, matching the reference
    // screenshots exactly (Edward, 2026).
    //
    // Model indices are PICKMOD.BND indices (see Formats/ModelIndices.h's
    // PickMod namespace) - NOT yet confirmed against a real PICKMOD.BND
    // file, same starting point OPTOBJ's indices had before that got
    // verified. Weapons carry a secondaryModelIndex too (the ammo type
    // model, per Edward: "Weapons have two spinning models on screen,
    // the weapon and the ammunition type").
    //
    // Runtime state (which items are actually available/equipped, ammo
    // counts) is NOT part of this static tree - see PlayerInventoryState,
    // queried separately at render time by label.
    ALTEngine::Menu::MenuNode BuildPauseMenuTree(ALTEngine::Bootstrap::Language language);
}
