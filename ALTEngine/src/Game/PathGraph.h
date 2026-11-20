#pragma once

#include "../Formats/LevelLoader.h"

#include <optional>
#include <vector>

namespace ALTEngine::Game
{
    // The level's enemy path network, as a graph.
    //
    // DATA (checked on the demo disc's L111/L112): each PathNode sits on a grid
    // cell and names up to four neighbours in nodeA..nodeD, 0xFF for none.
    // Every link is in range and appears in both directions (156 and 168 link
    // ends respectively, none one-way), so it is an undirected graph.
    //
    // nodeState is carried through but not acted on. It is not a state: it
    // is the node's half-cell offset (bits 1 and 2 add 0x100 on x and z), as
    // FlowField::Position reads it (FUN_00033114 / FUN_00033234).
    //
    // A DATA CHECK ONLY, NOT THE AI. The creatures follow the original's own
    // flow field over these nodes (FlowField.h, FUN_000281cc); this graph is
    // kept for its tests, which check the network's shape on the demo levels.
    class PathGraph
    {
    public:
        struct Node
        {
            int cellX = 0;
            int cellZ = 0;
            int state = 0;
            std::vector<int> neighbours;
        };

        PathGraph() = default;
        explicit PathGraph(const Formats::LevelGeometry& level);

        const std::vector<Node>& Nodes() const { return nodes_; }
        bool Empty() const { return nodes_.empty(); }

        // The node nearest a cell, by straight-line grid distance.
        std::optional<int> NearestNode(int cellX, int cellZ) const;

        // Shortest route from one node to another, by grid distance along the
        // links, both ends included. Empty if unreachable.
        std::vector<int> Route(int from, int to) const;

        // The first node to head for on the way from `from` to `to` - `to`
        // itself when they are adjacent, `from` when they are the same node.
        // nullopt if unreachable.
        std::optional<int> NextHop(int from, int to) const;

        // How many separate pieces the network is in.
        int ComponentCount() const;

    private:
        std::vector<Node> nodes_;
    };
}
