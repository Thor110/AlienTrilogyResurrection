#include "PathGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace ALTEngine::Game
{
    PathGraph::PathGraph(const Formats::LevelGeometry& level)
    {
        const int count = static_cast<int>(level.pathNodes.size());
        nodes_.resize(level.pathNodes.size());
        for (int i = 0; i < count; ++i)
        {
            const auto& source = level.pathNodes[static_cast<size_t>(i)];
            Node& node = nodes_[static_cast<size_t>(i)];
            node.cellX = source.x;
            node.cellZ = source.y;
            node.state = source.nodeState;
            for (int link : { source.nodeA, source.nodeB, source.nodeC, source.nodeD })
            {
                if (link == 0xFF || link >= count || link == i) { continue; }
                if (std::find(node.neighbours.begin(), node.neighbours.end(), link) == node.neighbours.end())
                {
                    node.neighbours.push_back(link);
                }
            }
        }
    }

    std::optional<int> PathGraph::NearestNode(int cellX, int cellZ) const
    {
        std::optional<int> best;
        long bestDistance = std::numeric_limits<long>::max();
        for (size_t i = 0; i < nodes_.size(); ++i)
        {
            const long dx = nodes_[i].cellX - cellX;
            const long dz = nodes_[i].cellZ - cellZ;
            const long d = dx * dx + dz * dz;
            if (d < bestDistance) { bestDistance = d; best = static_cast<int>(i); }
        }
        return best;
    }

    std::vector<int> PathGraph::Route(int from, int to) const
    {
        const int count = static_cast<int>(nodes_.size());
        if (from < 0 || to < 0 || from >= count || to >= count) { return {}; }

        // Dijkstra on grid distance - the graph is a few dozen nodes, so a
        // plain binary heap is plenty.
        std::vector<float> distance(nodes_.size(), std::numeric_limits<float>::infinity());
        std::vector<int> previous(nodes_.size(), -1);
        using Entry = std::pair<float, int>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
        distance[static_cast<size_t>(from)] = 0.0f;
        open.push({ 0.0f, from });

        while (!open.empty())
        {
            const auto [d, at] = open.top();
            open.pop();
            if (d > distance[static_cast<size_t>(at)]) { continue; }
            if (at == to) { break; }
            const Node& node = nodes_[static_cast<size_t>(at)];
            for (int next : node.neighbours)
            {
                const Node& other = nodes_[static_cast<size_t>(next)];
                const float step = std::hypot(static_cast<float>(other.cellX - node.cellX),
                                              static_cast<float>(other.cellZ - node.cellZ));
                if (d + step < distance[static_cast<size_t>(next)])
                {
                    distance[static_cast<size_t>(next)] = d + step;
                    previous[static_cast<size_t>(next)] = at;
                    open.push({ d + step, next });
                }
            }
        }

        if (distance[static_cast<size_t>(to)] == std::numeric_limits<float>::infinity()) { return {}; }
        std::vector<int> route;
        for (int at = to; at != -1; at = previous[static_cast<size_t>(at)]) { route.push_back(at); }
        std::reverse(route.begin(), route.end());
        return route;
    }

    std::optional<int> PathGraph::NextHop(int from, int to) const
    {
        const std::vector<int> route = Route(from, to);
        if (route.empty()) { return std::nullopt; }
        return route.size() == 1 ? route[0] : route[1];
    }

    int PathGraph::ComponentCount() const
    {
        std::vector<bool> seen(nodes_.size(), false);
        int components = 0;
        for (size_t start = 0; start < nodes_.size(); ++start)
        {
            if (seen[start]) { continue; }
            components++;
            std::vector<int> stack{ static_cast<int>(start) };
            seen[start] = true;
            while (!stack.empty())
            {
                const int at = stack.back();
                stack.pop_back();
                for (int next : nodes_[static_cast<size_t>(at)].neighbours)
                {
                    if (!seen[static_cast<size_t>(next)]) { seen[static_cast<size_t>(next)] = true; stack.push_back(next); }
                }
            }
        }
        return components;
    }
}
