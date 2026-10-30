#include "TestFramework.h"

#include <algorithm>

#include "Screens/Enemies.h"
#include "TestWorld.h"

using AltTest::OpenWorld;

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
        e.ai.type = type;
        e.ai.speed = 0x60;
        e.ai.wakeRange = 0x1000;
        return e;
    }

    // One game tick for a creature on flat ground: think, move, then settle.
    // The player at the origin occupies the cell from -256 to 256, and a step
    // whose 200-unit lead probe would enter it is refused, as GameplayScreen's
    // probe does.
    void Step(Enemies::Enemy& e)
    {
        Enemies::Tick(e, 0, 0, 0, OpenWorld());
        const float lead = (e.stepZ >= 0.0f) ? 200.0f : -200.0f;
        const float probe = e.z + e.stepZ + lead;
        if (!(probe > -256.0f && probe < 256.0f)) { e.z += e.stepZ; }
        e.x += e.stepX;
        Enemies::SettleVertical(e, 0.0f, 100000.0f, 0x200);
    }
}

namespace
{
    // Steps a creature until `done` holds, or `limit` ticks pass. Returns the
    // ticks taken, or -1.
    template <typename Done>
    int StepUntil(Enemies::Enemy& e, Done done, int limit = 600)
    {
        for (int t = 0; t < limit; ++t)
        {
            if (done(e)) { return t; }
            Step(e);
        }
        return done(e) ? limit : -1;
    }
}

TEST_CASE("Enemies: only one hugger pounces at a time")
{
    // The first hugger to attack from contact range takes the face-hug lock;
    // a second, finding it taken, leaps instead (FUN_00033cbc).
    Enemies::ResetLevelState();
    Enemies::Enemy a = HuggerAtPlayer();
    REQUIRE(StepUntil(a, [](const Enemies::Enemy& e) { return e.pouncing; }) >= 0);
    CHECK(Enemies::pounceLockHeld);
    Enemies::Enemy b = HuggerAtPlayer();
    REQUIRE(StepUntil(b, [](const Enemies::Enemy& e) { return e.leaping || e.pouncing; }) >= 0);
    CHECK(!b.pouncing);
    CHECK(b.leaping);
    Enemies::ResetLevelState();
}

TEST_CASE("Enemies: a level reload releases a pounce that never finished")
{
    // Dying or quitting mid-pounce leaves the pounce lock held. Before
    // ResetLevelState existed, no hugger could pounce again all session.
    Enemies::ResetLevelState();
    Enemies::Enemy first = HuggerAtPlayer();
    REQUIRE(StepUntil(first, [](const Enemies::Enemy& e) { return e.pouncing; }) >= 0);

    Enemies::ResetLevelState();   // what GameplayScreen does on every load
    Enemies::Enemy next = HuggerAtPlayer();
    CHECK(StepUntil(next, [](const Enemies::Enemy& e) { return e.pouncing; }) >= 0);
    Enemies::ResetLevelState();
}

TEST_CASE("Enemies: a hugger only attacks once it has closed to contact range")
{
    // A hugger in the middle band walks in; it does not leap from there.
    Enemies::ResetLevelState();
    Enemies::Enemy e = HuggerAtPlayer(1200.0f);
    Enemies::Tick(e, 0, 0, 0, OpenWorld());
    CHECK_EQ(e.mode, 1);   // 1200 is inside the clamped 0x600 near threshold
    CHECK(!e.leaping);
    CHECK(!e.pouncing);
}

TEST_CASE("Enemies: the leap rises, falls under gravity and lands at floor + 0x20")
{
    // With the face-hug lock taken, a hugger in contact range leaps.
    Enemies::ResetLevelState();
    Enemies::pounceLockHeld = true;
    Enemies::Enemy e = HuggerAtPlayer(900.0f);
    REQUIRE(StepUntil(e, [](const Enemies::Enemy& c) { return c.leaping; }) >= 0);
    CHECK(!e.pouncing);
    float highest = 0.0f;
    int airborne = 0;
    while (e.leaping && airborne < 200)
    {
        highest = std::max(highest, e.y);
        ++airborne;
        Step(e);
    }
    CHECK(!e.leaping);
    CHECK(airborne > 5);
    CHECK(highest > static_cast<float>(EnemyBehaviour::STAND_CLEARANCE));
    CHECK_EQ(e.y, static_cast<float>(EnemyBehaviour::STAND_CLEARANCE));
    Enemies::ResetLevelState();
}

TEST_CASE("Enemies: nothing attacks from the far bands")
{
    Enemies::ResetLevelState();
    Enemies::Enemy e = HuggerAtPlayer(3000.0f);
    Enemies::Tick(e, 0, 0, 0, OpenWorld());
    CHECK(e.mode >= EnemyBehaviour::FAR_MODE);
    CHECK(!e.leaping);
    CHECK(!e.pouncing);
}

TEST_CASE("Enemies: an attack waits until the creature faces the player exactly")
{
    Enemies::ResetLevelState();
    Enemies::Enemy e = HuggerAtPlayer(512.0f, 3);
    e.facing = 0x400;   // a quarter turn off
    Enemies::Tick(e, 0, 0, 0, OpenWorld());
    CHECK(!e.leaping);
    int ticks = 1;
    while (!e.leaping && ticks < 50) { Enemies::Tick(e, 0, 0, 0, OpenWorld()); ++ticks; }
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
        Enemies::Tick(e, 0, 0, 0, world, false);
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
            Enemies::Tick(e, 0, 0, 0, world, false);
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
        if (creature < 0) { continue; }
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

TEST_CASE("Hits: a warrior is pushed away while it flinches, then attacks again")
{
    // FUN_000310ac: state 6 and 0x10 ticks of immunity, pushed at a quarter of
    // its speed along its bearing turned half round; at the end (0x34ba4) it
    // stops and, with a line to the player, goes straight back into an attack.
    Enemies::ResetLevelState();
    CreatureAI::World world;
    uint32_t seed = 7;
    world.cellOpenOrPlayer = [](int, int) { return true; };
    world.cellOpen = [](int, int) { return true; };
    world.lineTo = [](int, int, int) { return true; };
    world.nodeAt = [](int, int) { return -1; };
    world.nodeToward = [](int) { return -1; };
    world.nodeAway = [](int) { return -1; };
    world.nodePosition = [](int, int&, int&) { return false; };
    world.random = [&]() { seed = seed * 1103515245u + 12345u; return static_cast<int>((seed >> 16) & 0xffff); };

    Enemies::Enemy e = HuggerAtPlayer(900.0f, 6);
    e.ai.type = 6;
    e.ai.speed = 0x40;
    e.lineOfFire = true;
    Enemies::Tick(e, 0, 0, 0, world, false);   // sets its bearing to the player

    CHECK(!Enemies::ApplyHit(e, 5));
    CHECK(e.state == EnemyBehaviour::STATE_HIT_RECOVER);
    CHECK_EQ(e.hitCooldown, EnemyBehaviour::HIT_IMMUNITY);
    CHECK(e.pushZ > 0.0f);                     // the player is toward -z
    CHECK(std::fabs(e.pushZ - 0x10) < 1.0f);   // 0x40 >> 2
    CHECK(!Enemies::ApplyHit(e, 5));           // immune
    CHECK_EQ(e.health, 59);

    const float z = e.z;
    int t = 0;
    for (; t < 200 && e.state == EnemyBehaviour::STATE_HIT_RECOVER; ++t)
    {
        Enemies::Tick(e, 0, 0, 0, world, false);
        e.x += e.stepX; e.z += e.stepZ;
    }
    CHECK(e.state == EnemyBehaviour::STATE_ACTING);
    CHECK(e.z > z);
    CHECK(e.pushX == 0.0f && e.pushZ == 0.0f);
    Enemies::Tick(e, 0, 0, 0, world, false);
    CHECK(e.ai.state == CreatureAI::ACT);
    Enemies::ResetLevelState();
}

TEST_CASE("Hits: the ceiling creatures never react, and flinch animations follow the view")
{
    Enemies::Enemy e = HuggerAtPlayer(900.0f, 8);
    CHECK(!Enemies::ApplyHit(e, 5));
    CHECK(e.state == EnemyBehaviour::STATE_ACTING);
    CHECK_EQ(e.hitCooldown, 0);
    CHECK_EQ(e.health, 59);

    CHECK_EQ(EnemyBehaviour::HitAnimation(2, 4), 8);
    CHECK_EQ(EnemyBehaviour::HitAnimation(6, 0), 8);
    CHECK_EQ(EnemyBehaviour::HitAnimation(6, 4), 9);
    CHECK_EQ(EnemyBehaviour::HitAnimation(1, 0, 0), 3);
    CHECK_EQ(EnemyBehaviour::HitAnimation(1, 0, 2), 4);
    CHECK_EQ(EnemyBehaviour::HitAnimation(1, 0, 4), -1);
}

TEST_CASE("Moves: a refused step slides up to the wall, and a step off a ledge hops down")
{
    namespace EB = EnemyBehaviour;
    // Heading +x from 1000 with a wall cell from 1536: the lead probe at
    // 1000 + 400 + 200 hits it, so it stops 201 short.
    CHECK_EQ(EB::BlockedStep(1000, 400), 1536 - 201);
    // Heading -x from 1400 with the wall cell 512..1023: 1024 + 200.
    CHECK_EQ(EB::BlockedStep(1400, -300), 1224);

    float vy = 0.0f;
    CHECK_EQ(EB::LedgeStep(1030, 30, 0x400, 0x300, false, vy), 1030);   // 0x100 is a step, not a drop
    CHECK(vy == 0.0f);
    CHECK_EQ(EB::LedgeStep(1030, 30, 0x400, 0x200, false, vy), 1024 + 200);
    CHECK(vy == static_cast<float>(EB::LEDGE_HOP_LIFT));
    vy = -12.0f;
    CHECK_EQ(EB::LedgeStep(800, -30, 0x400, 0x200, false, vy), 800);    // already well in
    CHECK(vy == -12.0f);                                                // falling: no lift
    CHECK_EQ(EB::LedgeStep(1010, -30, 0x400, 0x200, false, vy), 512 + 312);
}

TEST_CASE("Deaths: a warrior slides back as it falls, then a ramp carries its body")
{
    namespace EB = EnemyBehaviour;
    Enemies::Enemy e = HuggerAtPlayer(900.0f, 6);
    e.ai.speed = 0x40;
    e.ai.bearingToPlayer = Enemies::BearingTo(e, 0, 0);
    e.health = 1;
    CHECK(Enemies::ApplyHit(e, 5));
    Enemies::Tick(e, 0, 0, 0, OpenWorld());
    CHECK(e.state == EB::STATE_DEAD);
    CHECK(std::fabs(e.stepZ - 0x10) < 1.0f);   // away from the player, at speed / 4
    const float z = e.z;
    int t = 0;
    for (; t < 400 && !e.bodyDown; ++t) { Step(e); }
    CHECK(e.bodyDown);
    CHECK(e.z > z);
    Step(e);
    CHECK(e.stepX == 0.0f && e.stepZ == 0.0f);   // down, on ordinary floor: still

    // On a ramp cell it is carried at full speed, and stops off the ramp.
    Enemies::Enemy r = HuggerAtPlayer(900.0f, 6);
    r.ai.speed = 0x40;
    r.health = 1;
    r.standingAttribute = 0x2e;
    CHECK(Enemies::ApplyHit(r, 5));
    Enemies::Tick(r, 0, 0, 0, OpenWorld());
    CHECK(r.stepX == 0.0f && r.stepZ == 0.0f);   // no death push on a ramp
    for (t = 0; t < 400 && !r.bodyDown; ++t) { Step(r); }
    CHECK(r.stepX == 64.0f);
    r.standingAttribute = 0;
    Step(r);
    CHECK(r.stepX == 0.0f);

    int vx, vz;
    CHECK(EB::RampCarry(0x2d, 10, vx, vz)); CHECK(vz == -10);
    CHECK(EB::RampCarry(0x33, 10, vx, vz)); CHECK(vx == -10);
    CHECK(EB::RampCarry(0x34, 10, vx, vz)); CHECK(vz == 10);
    CHECK(!EB::RampCarry(0x35, 10, vx, vz));
    Enemies::ResetLevelState();
}

TEST_CASE("Deaths: a colonist's body looks the way the player sees it")
{
    namespace CO = CreatureTasks::Colonist;
    Enemies::Enemy e = HuggerAtPlayer(900.0f, 10);
    e.health = 1;
    CHECK(Enemies::ApplyHit(e, 5));
    for (int t = 0; t < 600 && !e.bodyDown; ++t) { Enemies::Tick(e, 0, 0, 0, OpenWorld()); }
    CHECK(e.bodyDown);
    const int view = CO::ViewOf(e.facingOctant, Enemies::BearingTo(e, 0, 0));
    CHECK_EQ(e.animation, CO::ANIM_BODY + view);
    // The player walks round to its other side.
    Enemies::Tick(e, 0, 0, 1800, OpenWorld());
    const int behind = CO::ViewOf(e.facingOctant, Enemies::BearingTo(e, 0, 1800));
    CHECK(behind != view);
    CHECK_EQ(e.animation, CO::ANIM_BODY + behind);
}

namespace
{
    CreatureAI::World OpenWorld(uint32_t& seed)
    {
        CreatureAI::World world;
        world.cellOpenOrPlayer = [](int, int) { return true; };
        world.cellOpen = [](int, int) { return true; };
        world.lineTo = [](int, int, int) { return true; };
        world.nodeAt = [](int, int) { return -1; };
        world.nodeToward = [](int) { return -1; };
        world.nodeAway = [](int) { return -1; };
        world.nodePosition = [](int, int&, int&) { return false; };
        world.random = [&seed]() { seed = seed * 1103515245u + 12345u; return static_cast<int>((seed >> 16) & 0xffff); };
        return world;
    }
}

TEST_CASE("Ceiling creatures: hang under the ceiling, and let go as warriors when they attack")
{
    Enemies::ResetLevelState();
    uint32_t seed = 11;
    CreatureAI::World world = OpenWorld(seed);
    constexpr float CEILING = 0x700;
    Enemies::Enemy e = HuggerAtPlayer(900.0f, 8);
    e.ai.type = 8;
    e.ai.speed = 0x40;
    e.ai.wakeRange = 0x1000;
    e.ai.alerted = true;
    e.lineOfFire = true;
    e.zoneVisible = true;
    e.onCeiling = e.ai.ceilingCrawler = true;
    Enemies::SettleVertical(e, 0.0f, CEILING, 0x100);
    CHECK(e.y == CEILING - EnemyBehaviour::STAND_CLEARANCE);

    bool dropped = false;
    for (int t = 0; t < 600 && !dropped; ++t)
    {
        Enemies::Tick(e, 0, 0, 0, world, false);
        e.x += e.stepX; e.z += e.stepZ;
        Enemies::SettleVertical(e, 0.0f, CEILING, 0x100);
        if (e.onCeiling) { CHECK(e.y == CEILING - EnemyBehaviour::STAND_CLEARANCE); }
        dropped = !e.onCeiling;
    }
    REQUIRE(dropped);
    CHECK_EQ(e.type, 6);                     // a warrior now
    CHECK(!e.ai.ceilingCrawler);
    CHECK_EQ(e.animation, EnemyBehaviour::CEILING_WARRIOR_DROP_ANIMATION);
    CHECK(e.y == EnemyBehaviour::STAND_CLEARANCE);   // on the floor
    Enemies::ResetLevelState();
}

TEST_CASE("Ceiling creatures: one killed up there falls, and No Hit Immunity lets every hit land")
{
    Enemies::Enemy dog = HuggerAtPlayer(900.0f, 9);
    dog.onCeiling = dog.ai.ceilingCrawler = true;
    dog.health = 1;
    CHECK(Enemies::ApplyHit(dog, 5));
    Enemies::Tick(dog, 0, 0, 0, OpenWorld());
    CHECK(!dog.onCeiling);
    CHECK_EQ(dog.type, 9);                   // its own death, on its own sprites
    Enemies::SettleVertical(dog, 0.0f, 0x700, 0x100);
    CHECK(dog.y == EnemyBehaviour::STAND_CLEARANCE);

    // A warrior mid-flinch: immune, unless the option is on; and dying is
    // never hit again.
    Enemies::Enemy w = HuggerAtPlayer(900.0f, 6);
    CHECK(!Enemies::ApplyHit(w, 5));
    CHECK(!Enemies::ApplyHit(w, 5));
    CHECK_EQ(w.health, 59);
    CHECK(!Enemies::ApplyHit(w, 5, true));
    CHECK_EQ(w.health, 54);
    CHECK(Enemies::ApplyHit(w, 100, true));
    CHECK(!Enemies::ApplyHit(w, 100, true));
}
