#pragma once

#include <string>
#include <vector>

namespace ALTEngine::Menu
{
    enum class MenuNodeKind
    {
        List,           // has children -> Enter opens a new column to the right
        Slider,         // Volume-style: rendered as bars, not a selectable child list
        CreditsScroll,  // opens the scrolling credits screen entirely
        Action,         // leaf - Enter performs an action, doesn't open a column
    };

    struct MenuNode
    {
        std::string label;
        MenuNodeKind kind = MenuNodeKind::List;

        // OPTOBJ model index (into the eventual real M0 section array),
        // NOT yet confirmed against real data - see MenuTree.cpp for the
        // provisional numbering (assigned in the order ModelRenderer.cs's
        // comment catalogs the known identifiers, which is not
        // necessarily on-disk section order). -1 = not set / inherit
        // parent's (see MenuNavigation's path-walk); if nothing along the
        // whole path sets one, no model is shown (e.g. Main Menu items).
        //
        // Using an int rather than a name specifically because it maps
        // directly onto AssetResolver's override convention once a real
        // OPTOBJ loader exists: override lookup becomes "OPTOBJ_{index:02}"
        // - no separate name<->index table needed at runtime. The
        // human-readable mapping (which index is "Computer", "Keyboard",
        // etc) lives in documentation for override authors, not in code.
        int modelIndex = -1;

        std::vector<MenuNode> children;

        // For Slider leaves: 0-10, matching the ~8/10 filled-bar look in
        // the reference images. Purely cosmetic placeholder for now - not
        // wired to actual audio.
        int sliderValue = 8;
    };
}
