#include "TestFramework.h"

#include "Formats/LevelLoader.h"
#include "Game/PathGraph.h"

#include <algorithm>

using namespace ALTEngine::Formats;
using namespace ALTEngine::Game;

TEST_CASE("PathGraph: the demo networks load as undirected graphs")
{
    for (const char* file : { "SECT11/L111LEV.MAP", "SECT11/L112LEV.MAP" })
    {
        LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile(file));
        PathGraph graph(level);
        REQUIRE(graph.Nodes().size() == level.pathNodes.size());
        for (size_t i = 0; i < graph.Nodes().size(); ++i)
        {
            for (int next : graph.Nodes()[i].neighbours)
            {
                const auto& back = graph.Nodes()[static_cast<size_t>(next)].neighbours;
                CHECK(std::find(back.begin(), back.end(), static_cast<int>(i)) != back.end());
            }
        }
    }
}

TEST_CASE("PathGraph: every node on L111 can reach every other")
{
    LevelGeometry level = LevelLoader::Load(AltTest::RequireDemoFile("SECT11/L111LEV.MAP"));
    PathGraph graph(level);
    CHECK_EQ(graph.ComponentCount(), 1);
    const int last = static_cast<int>(graph.Nodes().size()) - 1;
    const auto route = graph.Route(0, last);
    REQUIRE(!route.empty());
    CHECK_EQ(route.front(), 0);
    CHECK_EQ(route.back(), last);
    for (size_t i = 1; i < route.size(); ++i)
    {
        const auto& links = graph.Nodes()[static_cast<size_t>(route[i - 1])].neighbours;
        CHECK(std::find(links.begin(), links.end(), route[i]) != links.end());
    }
}

TEST_CASE("PathGraph: routes take the shorter way round")
{
    // A square 0-1-2-3-0 with one long side: 0 to 2 should go round the short way.
    LevelGeometry level;
    level.pathNodes = {
        { 0, 0, 0, 0, 1, 3, 0xFF, 0xFF },
        { 1, 0, 0, 0, 0, 2, 0xFF, 0xFF },
        { 1, 1, 0, 0, 1, 3, 0xFF, 0xFF },
        { 0, 20, 0, 0, 2, 0, 0xFF, 0xFF },
    };
    PathGraph graph(level);
    const std::vector<int> expected{ 0, 1, 2 };
    CHECK(graph.Route(0, 2) == expected);
    CHECK(graph.NextHop(0, 2) == std::optional<int>(1));
    CHECK(graph.NextHop(2, 2) == std::optional<int>(2));
    CHECK(graph.NearestNode(1, 2) == std::optional<int>(2));
}

TEST_CASE("PathGraph: unreachable and invalid routes are empty")
{
    LevelGeometry level;
    level.pathNodes = {
        { 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF },
        { 5, 5, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF },
    };
    PathGraph graph(level);
    CHECK_EQ(graph.ComponentCount(), 2);
    CHECK(graph.Route(0, 1).empty());
    CHECK(!graph.NextHop(0, 1).has_value());
    CHECK(graph.Route(0, 9).empty());
}
