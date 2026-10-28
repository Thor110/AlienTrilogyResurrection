#include "TestFramework.h"

#include "Game/FlowField.h"
#include "Screens/CreatureAI.h"

#include <cmath>

namespace AI = ALTEngine::Screens::CreatureAI;
using ALTEngine::Formats::LevelGeometry;
using ALTEngine::Formats::PathNode;

namespace
{
    // Open floor everywhere, every line clear, no path network.
    struct OpenWorld
    {
        AI::World world;
        uint32_t seed = 7;
        bool lines = true;
        OpenWorld()
        {
            world.cellOpenOrPlayer = [](int, int) { return true; };
            world.cellOpen = [](int, int) { return true; };
            world.lineTo = [this](int, int, int) { return lines; };
            world.nodeAt = [](int, int) { return -1; };
            world.nodeToward = [](int) { return -1; };
            world.nodeAway = [](int) { return -1; };
            world.nodePosition = [](int, int&, int&) { return false; };
            world.random = [this]() { seed = seed * 1103515245u + 12345u; return static_cast<int>((seed >> 16) & 0xffff); };
        }
    };

    // A creature at the origin with the player `distance` along -z (facing 0).
    AI::Agent Creature(int distance, int type = 4)
    {
        AI::Agent a;
        a.type = type;
        a.speed = 0x40;
        a.distance = distance;
        a.mode = AI::Band(a, distance);
        a.zoneVisible = true;
        a.lineToPlayer = true;
        a.bearingToPlayer = 0;
        a.wakeRange = 0x1800;
        return a;
    }

    void Tick(AI::Agent& a, OpenWorld& w, int px, int pz)
    {
        if (a.idleTimer > 0) { a.idleTimer--; }
        if (a.attackTimer > 0) { a.attackTimer--; }
        AI::Step(a, w.world, px, pz);
    }
}

TEST_CASE("CreatureAI: idle until the spawn timer runs out, then chase")
{
    OpenWorld w;
    AI::Agent a = Creature(0x1000);
    for (int i = 0; i < AI::SPAWN_IDLE_TIMER - 1; ++i) { Tick(a, w, 0, -0x1000); CHECK_EQ(a.state, AI::IDLE); }
    Tick(a, w, 0, -0x1000);
    CHECK_EQ(a.state, AI::CHASE);
}

TEST_CASE("CreatureAI: noticing the player, then heading for a point near them")
{
    OpenWorld w;
    AI::Agent a = Creature(0x1000);
    a.state = AI::CHASE;
    a.idleTimer = 0;
    // 0x1000 is outside half the wake range (0xc00) and the player is quiet:
    // not noticed, but seen in the cone with a clear line - alerted all the same.
    Tick(a, w, 0, -0x1000);
    CHECK(a.alerted);
    CHECK_EQ(a.state, AI::CHASE);
    Tick(a, w, 0, -0x1000);
    CHECK_EQ(a.state, AI::MOVE);
    const double fromPlayer = std::hypot(a.waypointX - 0.0, a.waypointZ + 0x1000);
    CHECK(fromPlayer >= 0x3f0);
    CHECK(fromPlayer <= 0x610);
}

TEST_CASE("CreatureAI: a creature that cannot see the player notices it by its noise")
{
    AI::Agent a = Creature(0x1000);
    a.wakeRange = 0x1400;
    CHECK(!AI::Notices(a));       // beyond half the wake range, and quiet
    a.playerMakingNoise = true;
    CHECK(AI::Notices(a));        // inside the whole of it while the gun is loud
}

TEST_CASE("CreatureAI: moving turns at 0x200 a tick and only sets off facing the waypoint")
{
    OpenWorld w;
    AI::Agent a = Creature(0x3000);
    a.state = AI::MOVE;
    a.attackTimer = 100;          // no attack interrupting
    a.facing = 0x700;             // facing nearly away (an exact half turn
                                  // goes the other way round, as the
                                  // original's sign extension does)
    a.waypointX = 0; a.waypointZ = -0x800;   // straight ahead of facing 0
    Tick(a, w, 0, -0x3000);
    CHECK_EQ(a.facing, 0x500);
    CHECK_EQ(a.vx, 0); CHECK_EQ(a.vz, 0);
    for (int i = 0; i < 3; ++i) { Tick(a, w, 0, -0x3000); }
    CHECK_EQ(a.facing, 0);
    CHECK_EQ(a.vx, 0);
    CHECK(a.vz < 0);              // setting off toward -z
}

TEST_CASE("CreatureAI: losing the line to the waypoint drops back to idle")
{
    OpenWorld w;
    w.lines = false;
    AI::Agent a = Creature(0x3000);
    a.state = AI::MOVE;
    a.attackTimer = 100;
    a.waypointX = 0; a.waypointZ = -0x800;
    Tick(a, w, 0, -0x3000);
    CHECK_EQ(a.state, AI::IDLE);
    CHECK_EQ(a.idleTimer, AI::LOST_LINE_IDLE);
}

TEST_CASE("CreatureAI: arriving means attacking; facing the player exactly means acting")
{
    OpenWorld w;
    AI::Agent a = Creature(0x300);
    a.state = AI::MOVE;
    a.waypointX = 0; a.waypointZ = -0x20;
    a.vz = -0x40;
    Tick(a, w, 0, -0x300);
    CHECK_EQ(a.state, AI::ATTACK);
    a.facing = 0x100;             // a little off
    Tick(a, w, 0, -0x300);
    CHECK_EQ(a.facing, 0);        // within one turn of it
    CHECK_EQ(a.state, AI::ACT);
    CHECK(a.startAct);
    CHECK_EQ(a.attackTimer, AI::ATTACK_TIMER);
}

TEST_CASE("CreatureAI: after the act, a hugger retreats and anything else goes again")
{
    OpenWorld w;
    AI::Agent hugger = Creature(0x300, 2);
    hugger.state = AI::ACT;
    hugger.actFinished = true;
    Tick(hugger, w, 0, -0x300);
    CHECK(hugger.resolveHit);
    CHECK(hugger.retreating);
    CHECK_EQ(hugger.state, AI::CHASE);

    AI::Agent warrior = Creature(0x300, 4);
    warrior.state = AI::ACT;
    warrior.actFinished = true;
    Tick(warrior, w, 0, -0x300);
    CHECK(warrior.resolveHit);
    CHECK(warrior.startAct);      // still in line: straight into the next one
    CHECK_EQ(warrior.state, AI::ACT);
}

TEST_CASE("CreatureAI: the arrival test is per moving axis")
{
    AI::Agent a;
    a.waypointX = 100; a.waypointZ = 0;
    a.vx = 0x40;
    CHECK(!AI::Arrived(a));
    a.x = 70;
    CHECK(AI::Arrived(a));
    a.vx = 0; a.vz = 0;
    CHECK(!AI::Arrived(a));
}

TEST_CASE("FlowField: a breadth-first fill from the player's node, and steps along it")
{
    // A chain 0 - 1 - 2 - 3, and 4 hanging off 2.
    LevelGeometry level;
    auto node = [](uint8_t x, uint8_t a, uint8_t b, uint8_t c) {
        PathNode n; n.x = x; n.y = 1; n.nodeA = a; n.nodeB = b; n.nodeC = c; n.nodeD = 0xff; return n;
    };
    level.pathNodes = { node(0, 1, 0xff, 0xff), node(1, 0, 2, 0xff), node(2, 1, 3, 4), node(3, 2, 0xff, 0xff), node(5, 2, 0xff, 0xff) };
    level.pathNodes[4].nodeState = 3;
    ALTEngine::Game::FlowField field;
    field.Reset(level);
    field.Update(0);
    CHECK_EQ(field.Value(0), 0xff);
    CHECK_EQ(field.Value(1), 0xfe);
    CHECK_EQ(field.Value(2), 0xfd);
    CHECK_EQ(field.Value(3), 0xfc);
    CHECK_EQ(field.Value(4), 0xfc);
    CHECK_EQ(field.Step(3, true), 2);     // toward the player
    CHECK_EQ(field.Step(2, true), 1);
    CHECK_EQ(field.Step(1, false), 2);    // away from it
    CHECK_EQ(field.Step(0, true), 0);     // already there
    int x = 0, z = 0;
    REQUIRE(field.Position(4, x, z));
    CHECK_EQ(x, 5 * 0x200 + 0x100);
    CHECK_EQ(z, 1 * 0x200 + 0x100);
}
