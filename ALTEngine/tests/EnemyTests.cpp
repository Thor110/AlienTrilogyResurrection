#include "TestFramework.h"

#include "Screens/Enemies.h"

using namespace ALTEngine::Screens;

namespace
{
    // A facehugger (subtype 2, the melee type) standing right next to the player.
    Enemies::Enemy HuggerAtPlayer()
    {
        Enemies::Enemy e;
        e.type = 2;
        e.active = true;
        e.alive = true;
        e.state = EnemyBehaviour::STATE_ACTING;
        e.health = 64;
        return e;
    }
}

TEST_CASE("Enemies: only one hugger pounces at a time")
{
    Enemies::ResetLevelState();
    Enemies::Enemy a = HuggerAtPlayer();
    Enemies::Enemy b = HuggerAtPlayer();
    Enemies::Tick(a, 0, 0, 0);
    Enemies::Tick(b, 0, 0, 0);
    CHECK(a.pouncing);
    CHECK(!b.pouncing);
}

TEST_CASE("Enemies: a level reload releases a pounce that never finished")
{
    // Dying or quitting mid-pounce leaves the pounce lock held. Before
    // ResetLevelState existed, no hugger could pounce again all session.
    Enemies::ResetLevelState();
    Enemies::Enemy first = HuggerAtPlayer();
    Enemies::Tick(first, 0, 0, 0);
    REQUIRE(first.pouncing);

    Enemies::ResetLevelState();   // what GameplayScreen now does on every load
    Enemies::Enemy next = HuggerAtPlayer();
    Enemies::Tick(next, 0, 0, 0);
    CHECK(next.pouncing);
}
