#include "TestFramework.h"

#include "Screens/CreatureTasks.h"
#include "Screens/Enemies.h"

namespace EG = ALTEngine::Screens::CreatureTasks::Egg;
namespace EN = ALTEngine::Screens::Enemies;

namespace
{
    EG::Input Far() { EG::Input in; in.mode = 4; in.animationEnded = true; return in; }
}

TEST_CASE("Egg: stays closed while the player is far, rechecking after a random wait")
{
    EG::Egg egg;
    EG::Output out;
    for (int i = 0; i < EG::SPAWN_IDLE_TIMER; ++i) { out = EG::Step(egg, Far()); }
    CHECK(egg.state == EG::CHECK);
    EG::Input far = Far();
    far.random = 0x85;   // & 0x7f = 5
    out = EG::Step(egg, far);
    CHECK(egg.state == EG::CLOSED);
    CHECK_EQ(out.startAnimation, EG::ANIM_CLOSED);
    CHECK_EQ(egg.idleTimer, EG::RECHECK_MIN + 5);
    CHECK(egg.phase == EG::PHASE_CLOSED);
}

TEST_CASE("Egg: opens inside the middle band, and lets its hugger out up close with a line")
{
    EG::Egg egg;
    egg.state = EG::CHECK;
    EG::Input in;
    in.mode = 2;
    auto out = EG::Step(egg, in);
    CHECK_EQ(out.startAnimation, EG::ANIM_OPENING);
    CHECK(egg.state == EG::OPENING);
    out = EG::Step(egg, in);                 // still opening
    CHECK(egg.state == EG::OPENING);
    in.animationEnded = true;
    out = EG::Step(egg, in);
    CHECK(egg.state == EG::OPEN);
    out = EG::Step(egg, in);                 // mode 2: not close enough
    CHECK(!out.release);
    in.mode = 1;
    in.lineToPlayer = false;
    out = EG::Step(egg, in);                 // no line
    CHECK(!out.release);
    in.lineToPlayer = true;
    out = EG::Step(egg, in);
    CHECK(out.release);
    CHECK(out.die);
    CHECK(egg.phase == EG::PHASE_RELEASED);
}

TEST_CASE("Egg: a hit opens a closed egg")
{
    EG::Egg egg;
    EG::Input in = Far();
    in.recoveredFromHit = true;
    const auto out = EG::Step(egg, in);
    CHECK_EQ(out.startAnimation, EG::ANIM_OPENING);
    CHECK(egg.state == EG::OPENING);
}

TEST_CASE("Egg: the released monster is alerted and leaping at the player")
{
    std::vector<EN::Enemy> roster(2);
    roster[0].monsterIndex = 0; roster[0].type = 1; roster[0].active = true;
    roster[1].monsterIndex = 1; roster[1].type = 2; roster[1].active = false;
    REQUIRE(EN::ReleaseFromEgg(roster, 1, 100.0f, 32.0f, 200.0f, 0x400));
    const auto& h = roster[1];
    CHECK(h.active);
    CHECK_EQ(h.facing, 0x400);
    CHECK(h.ai.alerted);
    CHECK(h.ai.state == ALTEngine::Screens::CreatureAI::ACT);
    CHECK(h.leaping);
    CHECK_EQ(h.vy, 0x60);
    CHECK(h.leapX > 63.0f);                  // 0x40 along a quarter turn: +x
    CHECK(!EN::ReleaseFromEgg(roster, 1, 0, 0, 0, 0));   // already out
    CHECK(!EN::ReleaseFromEgg(roster, 0xff, 0, 0, 0, 0));
}

#include "Screens/ParticleSystem.h"

namespace VT = ALTEngine::Screens::CreatureTasks::Vent;
namespace PT = ALTEngine::Screens::Particles;

namespace
{
    VT::Input VentIn()
    {
        VT::Input in;
        in.type = 0x11;            // horizontal flame: a jet every 3rd tick
        in.cycleLength = 40;
        in.stopAt = 20;
        in.stopLength = 9;         // quiet for cycle 20..29
        return in;
    }
}

TEST_CASE("Vent: jets on its cycle, quiet in its stopped stretch")
{
    VT::Vent v;
    VT::Input in = VentIn();
    VT::Output out = VT::Step(v, in);        // dormant with no health: starts jetting
    CHECK(v.state == VT::JETTING);
    CHECK(!out.jetting);
    int emits = 0, sounds = 0, jetTicks = 0, quietTicks = 0;
    for (int t = 0; t < 41; ++t)
    {
        out = VT::Step(v, in);
        if (out.emit) { ++emits; }
        if (out.startSound) { ++sounds; }
        if (out.jetting) { ++jetTicks; } else { ++quietTicks; }
    }
    CHECK_EQ(quietTicks, 10);
    CHECK_EQ(jetTicks, 31);
    CHECK_EQ(sounds, 2);                     // once at the start, again after the quiet
    CHECK(emits >= 9);
}

TEST_CASE("Vent: a pipe with health waits to be shot; a trigger switches it off once")
{
    VT::Vent v;
    VT::Input in = VentIn();
    in.hasHealth = true;
    for (int t = 0; t < 5; ++t) { CHECK(!VT::Step(v, in).jetting); }
    CHECK(v.state == VT::DORMANT);
    in.burst = true;
    CHECK(VT::Step(v, in).explode);
    CHECK(v.state == VT::JETTING);
    in.burst = false;
    in.triggeredOff = true;
    CHECK(VT::Step(v, in).switchOffNotice);
    CHECK(!VT::Step(v, in).switchOffNotice);
    CHECK(!VT::Step(v, in).jetting);
}

TEST_CASE("Vent: out of view or over 0x1e00 away it does nothing")
{
    VT::Vent v;
    v.state = VT::JETTING;
    VT::Input in = VentIn();
    in.visible = false;
    for (int t = 0; t < 10; ++t) { CHECK(!VT::Step(v, in).jetting); }
    in.visible = true;
    in.distance = VT::SIMULATION_CUTOFF + 1;
    CHECK(!VT::Step(v, in).jetting);
}

TEST_CASE("Vent jets: rise, hold under the ceiling, and scald the player")
{
    PT::Pool pool(8);
    auto* p = pool.SpawnVent(PT::TYPE_VENT_STEAM_V, 0, 0, 0, 0x41, 0, false);
    REQUIRE(p);
    CHECK_EQ(p->vy, 0x41);
    auto heights = [](float, float, float& f, float& c) { f = 0; c = 0x300; };
    int damage = 0;
    auto hurt = [&](int d) { damage += d; };
    // Player well away.
    for (int t = 0; t < 4; ++t) { pool.TickVents(heights, 5000, 0, 5000, hurt); }
    CHECK_EQ(p->y, 0x100);                   // 0x300 - 0x200: held there
    CHECK_EQ(p->vy, -10);
    CHECK(std::fabs(p->vx) >= 32);           // fanned out by half its speed or more

    // A horizontal flame straight into the player.
    PT::Pool pool2(8);
    auto* f = pool2.SpawnVent(PT::TYPE_VENT_FLAME_H, 0, 100, 0, 0x78, 0x400, false);
    REQUIRE(f);
    damage = 0;
    for (int t = 0; t < 4 && f->alive; ++t) { pool2.TickVents(heights, 300, 50, 0, hurt); }
    CHECK_EQ(damage, 0x0a);
    CHECK(!f->alive);
}

TEST_CASE("Monster trigger: a negative amount takes an active vent below its threshold")
{
    std::vector<EN::Enemy> roster(1);
    roster[0].monsterIndex = 4; roster[0].type = 0x12;
    roster[0].triggerThreshold = 2;
    CHECK(!EN::TriggerSpawn(roster, 4, 1));
    CHECK(EN::TriggerSpawn(roster, 4, 1));
    CHECK(roster[0].active);
    CHECK(!EN::TriggerSpawn(roster, 4, 1));  // at its threshold: refused
    CHECK_EQ(roster[0].triggerCount, 2);
    CHECK(!EN::TriggerSpawn(roster, 4, 0xff));   // -1
    CHECK_EQ(roster[0].triggerCount, 1);
    CHECK(roster[0].active);                 // still out; the vent itself switches off
}

namespace CO = ALTEngine::Screens::CreatureTasks::Colonist;

TEST_CASE("Colonist: which eighth the player is in, 4 straight in front")
{
    // The bearing (+0x54) runs in the same sense as the facing, so a player
    // on the colonist's own heading reads 4, and one behind it 0.
    for (int octant = 0; octant < 8; ++octant)
    {
        CHECK_EQ(CO::ViewOf(octant, octant * 0x200), CO::FRONT);
        CHECK_EQ(CO::ViewOf(octant, ((octant + 4) & 7) * 0x200), 0);
    }
}

TEST_CASE("Colonist: follows the player with its head, cries, and bursts up close")
{
    CO::Colonist c;
    CO::Input in;
    in.mode = 3;
    in.view = 2;
    auto out = CO::Step(c, in);
    CHECK_EQ(out.startAnimation, CO::ANIM_LOOK + 2);
    out = CO::Step(c, in);
    CHECK_EQ(out.startAnimation, -1);       // same view: nothing new

    c.idleTimer = 1;
    in.mode = 2;
    out = CO::Step(c, in);                   // timer clears: middle band, not in front
    CHECK(out.cry);
    CHECK(c.state == CO::CRYING);
    out = CO::Step(c, in);                   // not in front: back to looking
    CHECK(c.state == CO::LOOKING);

    c.idleTimer = 1;
    in.mode = 1;
    in.view = CO::FRONT;
    out = CO::Step(c, in);
    CHECK_EQ(out.startAnimation, CO::ANIM_BURSTING);
    CHECK(c.state == CO::BURSTING);
    out = CO::Step(c, in);
    CHECK(!out.burst);
    in.animationEvent = true;
    out = CO::Step(c, in);
    CHECK(out.burst);
    CHECK(c.state == CO::BURST);
}

TEST_CASE("Colonist: its burster comes out harder than a hugger from an egg")
{
    std::vector<EN::Enemy> roster(1);
    roster[0].monsterIndex = 3; roster[0].type = 3;
    auto* b = EN::ReleaseFromEgg(roster, 3, 0, 64, 0, 0, CO::BURSTER_SPEED, CO::BURSTER_LIFT);
    REQUIRE(b);
    CHECK_EQ(b->vy, 0x78);
    CHECK(std::fabs(b->leapZ + 0x60) < 0.5f);   // bearing 0: straight down -Z at 0x60
}
