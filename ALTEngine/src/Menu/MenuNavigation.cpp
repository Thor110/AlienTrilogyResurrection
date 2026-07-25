#include "MenuNavigation.h"

#include <stdexcept>
#include <string>

namespace ALTEngine::Menu
{
    const MenuNode& WalkPath(const MenuNode& root, const std::vector<int>& path)
    {
        const MenuNode* current = &root;
        for (int index : path)
        {
            if (index < 0 || static_cast<size_t>(index) >= current->children.size())
            {
                throw std::out_of_range("WalkPath: index out of range");
            }
            current = &current->children[static_cast<size_t>(index)];
        }
        return *current;
    }

    int EffectiveModelIndex(const MenuNode& root, const std::vector<int>& path)
    {
        int index = root.modelIndex;
        const MenuNode* current = &root;
        for (int childIndex : path)
        {
            if (childIndex < 0 || static_cast<size_t>(childIndex) >= current->children.size()) { break; }
            current = &current->children[static_cast<size_t>(childIndex)];
            if (current->modelIndex != -1) { index = current->modelIndex; }
        }
        return index;
    }

    void MoveSelection(const MenuNode& root, std::vector<int>& path, int delta)
    {
        if (path.empty()) { return; }

        std::vector<int> parentPath(path.begin(), path.end() - 1);
        const MenuNode& parent = WalkPath(root, parentPath);
        if (parent.children.empty()) { return; }

        int count = static_cast<int>(parent.children.size());
        int newIndex = (path.back() + delta + count) % count;
        path.back() = newIndex;
    }

    EnterResult Enter(const MenuNode& root, std::vector<int>& path)
    {
        const MenuNode& deepest = WalkPath(root, path);

        switch (deepest.kind)
        {
        case MenuNodeKind::List:
            if (!deepest.children.empty())
            {
                path.push_back(0);
                return EnterResult::Descended;
            }
            return EnterResult::NoOp;
        case MenuNodeKind::CreditsScroll:
            return EnterResult::EnteredCredits;
        case MenuNodeKind::Action:
            return EnterResult::Toggled;
        case MenuNodeKind::Slider:
        default:
            return EnterResult::NoOp;
        }
    }

    bool Back(std::vector<int>& path)
    {
        if (path.size() <= 1) { return false; }
        path.pop_back();
        return true;
    }
}
