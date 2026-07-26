#pragma once

#include <string>
#include <vector>

#include "MenuNode.h"

namespace ALTEngine::Menu
{
    // Walks `path` from `root`: path[i] indexes into the children of the
    // node reached after path[0..i-1]. Returns the deepest node reached.
    // Throws std::out_of_range on an invalid path (shouldn't happen if
    // callers only produce paths via MoveSelection/Enter/Back below).
    const MenuNode& WalkPath(const MenuNode& root, const std::vector<int>& path);

    // Mutable overload - used to update a settings list's
    // initialSelectedChild after the user picks a new value, so a later
    // preview or re-entry reflects what was actually just picked rather
    // than a stale boot-time snapshot (Edward, 2026).
    MenuNode& WalkPath(MenuNode& root, const std::vector<int>& path);

    // The deepest set modelIndex along `path` from `root`, else -1.
    int EffectiveModelIndex(const MenuNode& root, const std::vector<int>& path);

    // Moves the selection at the deepest level of `path` up/down within
    // its parent's children, wrapping around at either end (moving up
    // from the first item goes to the last, and vice versa). No-op if
    // `path` is empty.
    void MoveSelection(const MenuNode& root, std::vector<int>& path, int delta);

    enum class EnterResult
    {
        Descended,      // path was extended - now navigating a new column
        Toggled,        // deepest node was an Action leaf - caller applies its meaning
        EnteredCredits, // deepest node was CreditsScroll - caller switches screen
        NoOp,
    };

    // Applies Enter at the current path.
    EnterResult Enter(const MenuNode& root, std::vector<int>& path);

    // Pops the deepest level of `path` if more than one level deep and
    // returns true (still within this subtree). Returns false if `path`
    // was already at its shallowest level - caller should leave the
    // subtree entirely (e.g. Options -> Main Menu).
    bool Back(std::vector<int>& path);
}
