#pragma once

#include "../Formats/LevelLoader.h"

#include <cstdint>
#include <vector>

namespace ALTEngine::Game
{
    // THE PATH NETWORK AS A FLOW FIELD - FUN_000281cc.
    //
    // Each path node record is { x, z, value, flags, n0, n1, n2, n3 }. The
    // value byte is not level data: whenever the player's node changes (the
    // mover reads it from the player's cell, byte +6, at 0x3e24b), every value
    // is cleared and a breadth-first fill runs out from the player's node,
    // which gets 0xff, each hop one less. Creatures then follow it:
    // FUN_00033114 steps to the neighbour with the higher value (toward the
    // player), FUN_00033234 to the lower (away). A node's own cell is its byte
    // x/z times 0x200, plus 0x100 on x for flag bit 1 and on z for bit 2.
    //
    // The fill visits n0 unconditionally and n1..n3 only while the previous
    // one was not 0xff, exactly as the original's nested tests do.
    class FlowField
    {
    public:
        static constexpr uint8_t NONE = 0xff;

        void Reset(const Formats::LevelGeometry& level)
        {
            nodes_ = &level.pathNodes;
            values_.assign(level.pathNodes.size(), 0);
            source_ = -1;
        }

        // Rebuilds the field if the player has moved to another node.
        void Update(int playerNode)
        {
            if (!nodes_ || playerNode == source_) { return; }
            source_ = playerNode;
            values_.assign(nodes_->size(), 0);
            if (playerNode < 0 || static_cast<size_t>(playerNode) >= nodes_->size()) { return; }

            std::vector<int> queue{ playerNode };
            values_[static_cast<size_t>(playerNode)] = 0xff;
            for (size_t head = 0; head < queue.size(); ++head)
            {
                const auto& node = (*nodes_)[static_cast<size_t>(queue[head])];
                const uint8_t value = values_[static_cast<size_t>(queue[head])];
                auto visit = [&](uint8_t next) {
                    if (next >= nodes_->size()) { return; }
                    if (values_[next] != 0) { return; }
                    values_[next] = static_cast<uint8_t>(value - 1);
                    queue.push_back(next);
                };
                visit(node.nodeA);
                if (node.nodeB == NONE) { continue; }
                visit(node.nodeB);
                if (node.nodeC == NONE) { continue; }
                visit(node.nodeC);
                if (node.nodeD == NONE) { continue; }
                visit(node.nodeD);
            }
        }

        int Value(int node) const
        {
            return (node >= 0 && static_cast<size_t>(node) < values_.size()) ? values_[static_cast<size_t>(node)] : 0;
        }

        // FUN_00033114 (toward = true) and FUN_00033234 (toward = false): the
        // neighbour whose value beats the node's own, or the node itself.
        int Step(int node, bool toward) const
        {
            if (!nodes_ || node < 0 || static_cast<size_t>(node) >= nodes_->size()) { return -1; }
            const auto& n = (*nodes_)[static_cast<size_t>(node)];
            int best = node;
            int bestValue = Value(node);
            auto consider = [&](uint8_t next) {
                if (next >= nodes_->size()) { return; }
                const int v = Value(next);
                if (toward ? (v > bestValue) : (v < bestValue)) { best = next; bestValue = v; }
            };
            consider(n.nodeA);
            if (n.nodeB != NONE)
            {
                consider(n.nodeB);
                if (n.nodeC != NONE)
                {
                    consider(n.nodeC);
                    if (n.nodeD != NONE) { consider(n.nodeD); }
                }
            }
            return best;
        }

        // The node's position in grid space.
        bool Position(int node, int& x, int& z) const
        {
            if (!nodes_ || node < 0 || static_cast<size_t>(node) >= nodes_->size()) { return false; }
            const auto& n = (*nodes_)[static_cast<size_t>(node)];
            x = n.x * 0x200 + ((n.nodeState & 1) ? 0x100 : 0);
            z = n.y * 0x200 + ((n.nodeState & 2) ? 0x100 : 0);
            return true;
        }

    private:
        const std::vector<Formats::PathNode>* nodes_ = nullptr;
        std::vector<uint8_t> values_;
        int source_ = -1;
    };
}
