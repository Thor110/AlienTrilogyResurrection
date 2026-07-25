#pragma once

#include <string>
#include <utility>
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

    // Which BND pair modelIndex refers to. Needed because MenuNode
    // itself doesn't otherwise know which catalog its modelIndex was
    // assigned from - normally implied by which tree the node lives in
    // (the boot menu's Options tree is entirely OPTOBJ, the pause menu's
    // tree is entirely PICKMOD), but the pause menu's new Save Game/
    // Load Game entries need the Harddrive Left/Right OPTOBJ models
    // specifically, breaking that "one tree, one catalog" assumption -
    // Edward, 2026.
    enum class ModelSource
    {
        PickMod, // the default - matches every existing PauseMenuTree entry
        Optobj,
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

        // See ModelSource's own comment above. Defaults to PickMod,
        // matching every existing usage before this field existed - only
        // the pause menu's Save Game/Load Game entries set this to
        // Optobj.
        ModelSource modelSource = ModelSource::PickMod;

        // A second model index, alongside modelIndex above - added for
        // the pause menu, where weapons show TWO spinning models (the
        // weapon itself + its ammo type), not one. -1 = no second model
        // (the common case - most items only ever need modelIndex).
        int secondaryModelIndex = -1;

        std::vector<MenuNode> children;

        // For Slider leaves: 0-10, matching the ~8/10 filled-bar look in
        // the reference images. Purely cosmetic placeholder for now - not
        // wired to actual audio.
        int sliderValue = 8;
    };

    // Small builder helpers - originally local to MenuTree.cpp, factored
    // out here once PauseMenuTree.cpp needed the exact same pattern.
    inline MenuNode MakeAction(std::string label, int modelIndex = -1, int secondaryModelIndex = -1, ModelSource modelSource = ModelSource::PickMod)
    {
        MenuNode n;
        n.label = std::move(label);
        n.kind = MenuNodeKind::Action;
        n.modelIndex = modelIndex;
        n.secondaryModelIndex = secondaryModelIndex;
        n.modelSource = modelSource;
        return n;
    }

    inline MenuNode MakeList(std::string label, std::vector<MenuNode> children, int modelIndex = -1)
    {
        MenuNode n;
        n.label = std::move(label);
        n.kind = MenuNodeKind::List;
        n.modelIndex = modelIndex;
        n.children = std::move(children);
        return n;
    }

    inline MenuNode MakeSlider(std::string label)
    {
        MenuNode n;
        n.label = std::move(label);
        n.kind = MenuNodeKind::Slider;
        return n;
    }
}
