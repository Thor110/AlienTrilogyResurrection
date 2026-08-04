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

        // Which child index Enter() should select when first descending
        // into this list - defaults to 0 (the old, always-reset-to-first
        // behaviour). Settings lists (Difficulty, Camera Sway, Language,
        // Quality, Resolution) set this to whichever child matches the
        // currently saved value, computed when the tree is built, so
        // re-entering a list shows what's actually selected rather than
        // always jumping back to the first option (Edward, 2026).
        int initialSelectedChild = 0;

        // True only for lists whose initialSelectedChild represents a
        // real, persisted "current value" (Difficulty, Camera Sway,
        // Language, Quality, Resolution) - as opposed to pure navigation
        // lists (Volume, Controls) where index 0 is just "the first
        // item", not a meaningful selection. Controls whether the
        // one-ahead preview column (shown while hovering a List, before
        // Entering it) draws any highlight at all - Edward, 2026:
        // "Volume's list shouldn't have a highlight until entered as
        // neither Music or SFX are 'active' or selected."
        bool isSettingsList = false;

        // False for hardware Options hasn't been tested against yet
        // (Controls' Joystick/Gravis Grip/Gravis Pad/SpaceOrb 360/VFX-1) -
        // renders with a dark green background and dark green text
        // regardless of cursor position, and doesn't pulse even when
        // the cursor is on it (Edward, 2026: "we can leave them like
        // that for now as I have no idea when I can hook up or test
        // that hardware").
        bool enabled = true;

        // Which InputAction (Bootstrap/InputActions.h) this leaf
        // represents, as a plain int (matching modelIndex's own
        // "plain int, not a typed enum" convention here) - -1 for
        // everything that isn't a Redefine list entry. deviceIndex is
        // the DeviceKind (also Bootstrap/InputActions.h, also a plain
        // int) this binding applies to - Keyboard, Mouse, or (once
        // real hardware exists to test against) one of the other
        // peripherals (Edward, 2026 - redefine controls page, reused
        // per device via a single generic helper).
        int inputActionIndex = -1;
        int deviceIndex = -1;

        // Which StringId (Bootstrap/StringId.h) to actually display for
        // this node, looked up via Tr(stringId, language) - separate
        // from `label`, which stays the stable, English-based internal
        // identifier every existing comparison (ApplyLeafAction,
        // FindChildIndexByLabel, parentLabel == "Exit Game", etc)
        // already relies on. -1 means "no translation entry, just show
        // label as-is" - used for dynamic content that isn't a fixed
        // UI string (Resolution's "1920x1080", Redefine's own
        // "{action}: {binding}" labels) (Edward, 2026: "lay the
        // foundations for a language system").
        int stringId = -1;
        int descriptionStringId = -1; // shown while this entry is highlighted, -1 for none

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
}
