#include "TestFramework.h"

#include <algorithm>

#include "Screens/Enemies.h"

using namespace ALTEngine::Screens;

namespace
{
    // A facehugger (subtype 2, the melee type) in contact range, 512 units
    // along +z from a player at the origin and already facing them (facing 0
    // points toward -z).
    Enemies::Enemy HuggerAtPlayer(float distance = 512.0f, int type = 2)
    {
        Enemies::Enemy e;
        e.type = type;
        e.active = true;
        e.alive = true;
        e.state = EnemyBehaviour::STATE_ACTING;
        e.health = 64;
        e.z = distance;
        e.nearThreshold = 768;      // a hugger's own record values
        e.middleThreshold = 1536;
        return e;
    }

    // One game tick for a creature on flat ground: think, move, then settle.
    // The player at the origin occupies the cell from -256 to 256, and a step
    // whose 200-unit lead probe would enter it is refused, as GameplayScreen's
    // probe does.
    void Step(Enemies::Enemy& e)
    {
        Enemies::Tick(e, 0, 0, 0);
        const float lead = (e.stepZ >= 0.0f) ? 200.0f : -200.0f;
        const float probe = e.z + e.stepZ + lead;
        if (!(probe > -256.0f && probe < 256.0f)) { e.z += e.stepZ; }
        e.x += e.stepX;
        Enemies::SettleVertical(e, 0.0f, 100000.0f, 0x200);
    }
}

TEST_CASE("Enemies: only one hugger pounces at a time")
{
    // The second hugger, finding the lock taken, leaps instead.
    Enemies::ResetLevelState();
    Enemies::Enemy a = HuggerAtPlayer();
    Enemies::Enemy b = HuggerAtPlayer();
    Enemies::Tick(a, 0, 0, 0);
    Enemies::Tick(b, 0, 0, 0);
    CHECK(a.pouncing);
    CHECK(!b.pouncing);
    CHECK(b.leaping);
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

TEST_CASE("Enemies: a hugger in the middle band leaps rather than face-hugging")
{
    Enemies::ResetLevelState();
    Enemies::Enemy e = HuggerAtPlayer(1200.0f);
    Enemies::Tick(e, 0, 0, 0);
    CHECK_EQ(e.mode, 2);
    CHECK(!e.pouncing);
    CHECK(e.leaping);
    CHECK_EQ(e.vy, static_cast<float>(EnemyBehaviour::LEAP_LIFT));
    CHECK(e.stepZ < 0.0f);                       // toward the player
    CHECK_EQ(e.attackCooldown, Enemies::ATTACK_INTERVAL_TICKS);
}

TEST_CASE("Enemies: the leap rises, falls under gravity and lands at floor + 0x20")
{
    Enemies::ResetLevelState();
    Enemies::Enemy e = HuggerAtPlayer(1200.0f);
    float highest = 0.0f;
    int airborne = 0;
    Step(e);
    while (e.leaping && airborne < 100)
    {
        highest = std::max(highest, e.y);
        ++airborne;
        Step(e);
    }
    CHECK(!e.leaping);
    CHECK(airborne > 15);
    CHECK(airborne < 30);
    CHECK(highest > 600.0f);
    CHECK_EQ(e.y, static_cast<float>(EnemyBehaviour::STAND_CLEARANCE));
}

TEST_CASE("Enemies: the leap stops at the player's cell and its damage lands with it")
{
    // Type 3 never face-hugs: it leaps even from contact range.
    Enemies::ResetLevelState();
    Enemies::Enemy near = HuggerAtPlayer(512.0f, 3);
    Step(near);
    CHECK(near.leaping);
    CHECK(!near.pouncing);

    Enemies::Enemy e = HuggerAtPlayer(1400.0f, 3);
    int damage = 0, hits = 0, ticks = 0;
    Step(e);
    REQUIRE(e.leaping);
    while (e.leaping && ticks < 100)
    {
        CHECK_EQ(e.pendingDamage, 0);   // nothing while in the air
        Step(e);
        ++ticks;
    }
    if (e.pendingDamage > 0) { damage = e.pendingDamage; ++hits; }
    CHECK(e.z >= 456.0f);               // stopped short of the player's cell
    CHECK(e.z < 600.0f);
    CHECK_EQ(hits, 1);
    CHECK_EQ(damage, EnemyBehaviour::AttackForSubtype(3).closeDamage);
}

TEST_CASE("Enemies: nothing attacks from the far bands")
{
    Enemies::ResetLevelState();
    Enemies::Enemy e = HuggerAtPlayer(3000.0f);
    Enemies::Tick(e, 0, 0, 0);
    CHECK(e.mode >= EnemyBehaviour::FAR_MODE);
    CHECK(!e.leaping);
    CHECK(!e.pouncing);
}

TEST_CASE("Enemies: an attack waits until the creature faces the player exactly")
{
    Enemies::ResetLevelState();
    Enemies::Enemy e = HuggerAtPlayer(1200.0f);
    e.facing = 0x400;   // a quarter turn off
    Enemies::Tick(e, 0, 0, 0);
    CHECK(!e.leaping);
    int ticks = 1;
    while (!e.leaping && ticks < 50) { Enemies::Tick(e, 0, 0, 0); ++ticks; }
    CHECK(e.leaping);
    CHECK(ticks > 5);
}
