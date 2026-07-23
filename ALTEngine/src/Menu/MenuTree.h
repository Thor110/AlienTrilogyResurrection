#pragma once

#include <string>
#include <vector>

#include "MenuNode.h"

namespace ALTEngine::Menu
{
    // Builds the root menu tree: Main Menu (Start Game / Multiplayer /
    // Load Game / Options) with the full Options subtree (Volume,
    // Controls, Difficulty, Camera Sway, Graphics, Language, Credits)
    // attached under "Options" - matches the reference screenshots.
    // "Graphics" is new, not in the original game - contains "Quality"
    // (the authentic-vs-smoothed toggle) and "Resolution".
    //
    // `resolutionLabels` becomes the Resolution submenu's children (e.g.
    // "1920x1080", "1280x720") - passed in rather than queried here so
    // this stays SDL-free and unit-testable; MenuController queries the
    // real display modes and passes them in. Pass an empty vector (the
    // default) for testing - Resolution will just have no options.
    //
    // Model indices are provisional (see MenuTree.cpp's ModelIndex
    // namespace) - not resolved OPTOBJ section indices, since we don't
    // have OPTOBJ.BND to confirm real indices against. Sub-items not
    // visually confirmed in the reference set (Mouse's own redefine
    // screen, Gravis Pad/SpaceOrb 360/VFX-1 controller models) are
    // structurally present but their modelIndex is left unset (-1, falls
    // back to "Computer") since there's no reference image confirming
    // what they'd actually show.
    MenuNode BuildMainMenuTree(const std::vector<std::string>& resolutionLabels = {});
}
