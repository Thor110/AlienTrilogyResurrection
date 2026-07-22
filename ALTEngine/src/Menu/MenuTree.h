#pragma once

#include "MenuNode.h"

namespace ALTEngine::Menu
{
    // Builds the root menu tree: Main Menu (Start Game / Multiplayer /
    // Load Game / Options) with the full Options subtree (Volume,
    // Controls, Difficulty, Camera Sway, Render Quality, Language,
    // Credits) attached under "Options" - matches the reference
    // screenshots. "Render Quality" is new, not in the original game -
    // the authentic-vs-smoothed toggle.
    //
    // Model indices are provisional (see MenuTree.cpp's ModelIndex
    // namespace) - not resolved OPTOBJ section indices, since we don't
    // have OPTOBJ.BND to confirm real indices against. Sub-items not
    // visually confirmed in the reference set (Mouse's own redefine
    // screen, Gravis Pad/SpaceOrb 360/VFX-1 controller models) are
    // structurally present but their modelIndex is left unset (-1, falls
    // back to "Computer") since there's no reference image confirming
    // what they'd actually show.
    MenuNode BuildMainMenuTree();
}
