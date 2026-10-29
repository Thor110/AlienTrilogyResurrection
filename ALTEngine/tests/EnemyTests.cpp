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
        e.nearThreshold = 0x600;    // a hugger's 768, after the spawn clamp
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

TEST_CASE("Enemies: a hugger only attacks once it has closed to contact range")
{
    // The attack state is entered from mode 0 only. A hugger in the middle
    // band walks in; it does not leap from there.
    Enemies::ResetLevelState();
    Enemies::Enemy e = HuggerAtPlayer(1200.0f);
    Enemies::Tick(e, 0, 0, 0);
    CHECK_EQ(e.mode, 1);   // 1200 is inside the clamped 0x600 near threshold
    CHECK(!e.engaged);
    CHECK(!e.leaping);
    CHECK(!e.pouncing);
}

TEST_CASE("Enemies: with the face-hug lock taken, a hugger in contact range leaps")
{
    Enemies::ResetLevelState();
    Enemies::Enemy first = HuggerAtPlayer();
    Enemies::Tick(first, 0, 0, 0);
    REQUIRE(first.pouncing);

    Enemies::Enemy e = HuggerAtPlayer(900.0f);
    Enemies::Tick(e, 0, 0, 0);
    CHECK(e.engaged);
    CHECK(e.leaping);
    CHECK_EQ(e.vy, static_cast<float>(EnemyBehaviour::LEAP_LIFT));
    CHECK(e.stepZ < 0.0f);                       // toward the player
    CHECK_EQ(e.attackCooldown, Enemies::ATTACK_INTERVAL_TICKS);
}

TEST_CASE("Enemies: the leap rises, falls under gravity and lands at floor + 0x20")
{
    Enemies::ResetLevelState();
    Enemies::Enemy lockHolder = HuggerAtPlayer();
    Enemies::Tick(lockHolder, 0, 0, 0);
    Enemies::Enemy e = HuggerAtPlayer(900.0f);
    float highest = 0.0f;
    int airborne = 0;
    Step(e);
    REQUIRE(e.leaping);
    while (e.leaping && airborne < 100)
    {
        highest = std::max(highest, e.y);
        ++airborne;
        Step(e);
    }
    CHECK(!e.leaping);
    CHECK(!e.engaged);                           // a hugger goes back to the chase
    CHECK(airborne > 15);
    CHECK(airborne < 30);
    CHECK(highest > 600.0f);
    CHECK_EQ(e.y, static_cast<float>(EnemyBehaviour::STAND_CLEARANCE));
}

TEST_CASE("Enemies: the leap stops at the player's cell and its damage lands with it")
{
    // Type 3 never face-hugs: in contact range it always leaps.
    Enemies::ResetLevelState();
    Enemies::Enemy e = HuggerAtPlayer(900.0f, 3);
    Step(e);
    REQUIRE(e.leaping);
    CHECK(!e.pouncing);
    int ticks = 0;
    while (e.leaping && ticks < 100)
    {
        CHECK_EQ(e.pendingDamage, 0);   // nothing while in the air
        Step(e);
        ++ticks;
    }
    CHECK(e.z >= 456.0f);               // stopped short of the player's cell
    CHECK(e.z < 600.0f);
    CHECK_EQ(e.pendingDamage, EnemyBehaviour::AttackForSubtype(3).closeDamage);
    CHECK(e.engaged);                   // type 3 stays in the attack state
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
    Enemies::Enemy e = HuggerAtPlayer(512.0f, 3);
    e.facing = 0x400;   // a quarter turn off
    Enemies::Tick(e, 0, 0, 0);
    CHECK(!e.leaping);
    int ticks = 1;
    while (!e.leaping && ticks < 50) { Enemies::Tick(e, 0, 0, 0); ++ticks; }
    CHECK(e.leaping);
    CHECK(ticks > 5);
}

TEST_CASE("Enemies: the spawn clamps the near threshold and the attack cone")
{
    CHECK_EQ(EnemyBehaviour::ClampCone(0x80), 0x100);
    CHECK_EQ(EnemyBehaviour::ClampCone(0x200), 0x200);
    CHECK_EQ(EnemyBehaviour::ClampCone(0x900), 0x700);
    CHECK(EnemyBehaviour::ClampsRecordFields(2));
    CHECK(!EnemyBehaviour::ClampsRecordFields(0x11));
}

TEST_CASE("Enemies: with a world, the common task wakes, closes and leaps")
{
    // Open floor, every line clear: a hugger 1200 units off goes from idle to
    // chasing, is alerted, picks a point near the player, and leaps at it.
    Enemies::ResetLevelState();
    CreatureAI::World world;
    uint32_t seed = 3;
    world.cellOpenOrPlayer = [](int, int) { return true; };
    world.cellOpen = [](int, int) { return true; };
    world.lineTo = [](int, int, int) { return true; };
    world.nodeAt = [](int, int) { return -1; };
    world.nodeToward = [](int) { return -1; };
    world.nodeAway = [](int) { return -1; };
    world.nodePosition = [](int, int&, int&) { return false; };
    world.random = [&]() { seed = seed * 1103515245u + 12345u; return static_cast<int>((seed >> 16) & 0xffff); };

    Enemies::Enemy e = HuggerAtPlayer(1200.0f);
    e.ai.type = 2;
    e.ai.speed = 0x60;
    e.ai.wakeRange = 0x1000;
    bool leapt = false, moved = false;
    for (int t = 0; t < 400 && !leapt; ++t)
    {
        Enemies::Tick(e, 0, 0, 0, &world, false);
        if (e.stepX != 0.0f || e.stepZ != 0.0f) { moved = true; }
        e.x += e.stepX; e.z += e.stepZ;
        Enemies::SettleVertical(e, 0.0f, 100000.0f, 0x200);
        if (e.leaping) { leapt = true; }
    }
    CHECK(moved);
    CHECK(leapt);
    CHECK(e.ai.alerted);
}

TEST_CASE("Enemies: a scripted leap and a swipe both end, and the creature goes on")
{
    Enemies::ResetLevelState();
    CreatureAI::World world;
    uint32_t seed = 5;
    world.cellOpenOrPlayer = [](int, int) { return true; };
    world.cellOpen = [](int, int) { return true; };
    world.lineTo = [](int, int, int) { return true; };
    world.nodeAt = [](int, int) { return -1; };
    world.nodeToward = [](int) { return -1; };
    world.nodeAway = [](int) { return -1; };
    world.nodePosition = [](int, int&, int&) { return false; };
    world.random = [&]() { seed = seed * 1103515245u + 12345u; return static_cast<int>((seed >> 16) & 0xffff); };

    // A hugger (the pounce lock held, so it leaps rather than face-hugs) and a
    // warrior, each run long enough to attack and come out of it.
    for (int type : { 2, 6 })
    {
        Enemies::pounceLockHeld = (type == 2);
        Enemies::Enemy e = HuggerAtPlayer(900.0f);
        e.type = type;
        e.ai.type = type;
        e.ai.speed = 0x40;
        e.ai.wakeRange = 0x1000;
        // An act starts on entering the state, or when a finished one is
        // started again in place (a warrior at contact range swipes on).
        int acts = 0, actTicks = 0, longest = 0, previousState = -1, previousFrame = 0;
        bool sawAttackAnimation = false;
        for (int t = 0; t < 1500; ++t)
        {
            Enemies::Tick(e, 0, 0, 0, &world, false);
            e.x += e.stepX; e.z += e.stepZ;
            Enemies::SettleVertical(e, 0.0f, 100000.0f, 0x200);
            const bool inAct = e.ai.state == CreatureAI::ACT;
            const bool onAttack = inAct && e.actAnimation >= 0 && e.animation == e.actAnimation;
            if (onAttack) { sawAttackAnimation = true; }
            if (inAct && (previousState != CreatureAI::ACT || (onAttack && e.animationFrame < previousFrame)))
            {
                ++acts;
                actTicks = 0;
            }
            if (inAct) { longest = std::max(longest, ++actTicks); }
            previousState = e.ai.state;
            previousFrame = e.animationFrame;
        }
        CHECK(acts >= 2);              // it attacked, and attacked again
        CHECK(longest < 200);          // no act runs on for ever
        CHECK(sawAttackAnimation);
    }
    Enemies::ResetLevelState();
}

TEST_CASE("Death: every creature's death animation exists and ends, so its body settles")
{
    namespace AD = ALTEngine::Formats::EnemyAnimData;
    namespace AS = ALTEngine::Formats::EnemyAnimSequences;
    for (int type = 1; type <= 14; ++type)
    {
        const int animation = EnemyBehaviour::DeathAnimation(type);
        const int creature = AD::CreatureIndexForType(type);
        CHECK(creature >= 0);
        CHECK(animation < AD::CREATURES[creature].count);
        const auto seq = AS::Find(creature, animation);
        CHECK(seq.count > 0);
        CHECK(Enemies::SequenceCanEnd({ seq.data, seq.count }));
    }
}

TEST_CASE("Death: a killed warrior stays down on its death animation's last frame")
{
    Enemies::Enemy e = HuggerAtPlayer(4000.0f, 6);
    e.health = 1;
    CHECK(Enemies::ApplyHit(e, 5));
    for (int t = 0; t < 400; ++t) { Step(e); }
    CHECK(!e.alive);
    CHECK(e.state == EnemyBehaviour::STATE_DEAD);
    CHECK(e.animation == EnemyBehaviour::DeathAnimation(6));
    CHECK(e.animationEnded);
    CHECK(Enemies::BodyShown(e));
    const int frame = e.animationFrame;
    Step(e);
    CHECK(e.animationFrame == frame);
    CHECK(e.stepX == 0.0f && e.stepZ == 0.0f);
    Enemies::ResetLevelState();
}
