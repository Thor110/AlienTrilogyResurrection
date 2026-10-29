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
    EG::Input distant = Far();
    distant.random = 0x85;   // & 0x7f = 5
    out = EG::Step(egg, distant);
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

#include "Screens/EnemyBehaviour.h"
#include "Screens/GameOverSequence.h"

TEST_CASE("Death clips: each creature's attack passes its own cause")
{
    namespace EB = ALTEngine::Screens::EnemyBehaviour;
    namespace GO = ALTEngine::Screens::GameOverSequence;
    CHECK_EQ(EB::AttackForSubtype(2).cause, GO::CAUSE_ENEMY_ATTACK);
    CHECK_EQ(EB::AttackForSubtype(5).cause, GO::CAUSE_DOG);
    CHECK_EQ(EB::AttackForSubtype(6).cause, GO::CAUSE_WARRIOR);
    CHECK_EQ(EB::AttackForSubtype(7).cause, GO::CAUSE_QUEEN);
    // So a warrior drone's kill plays DEATH2 and the queen's DEATH3.
    CHECK_EQ(GO::DeathVideoForCause(GO::CAUSE_WARRIOR), 2);
    CHECK_EQ(GO::DeathVideoForCause(GO::CAUSE_QUEEN), 3);
}

TEST_CASE("Damage: a creature that survives a hit can be hit again once its reaction is over")
{
    EN::Enemy w;
    w.type = 6; w.active = true; w.alive = true; w.health = 480;
    w.x = 0; w.z = 0;
    REQUIRE(!EN::ApplyHit(w, 32));
    CHECK_EQ(w.health, 448);
    CHECK(!EN::ApplyHit(w, 32));             // still reacting: no damage
    CHECK_EQ(w.health, 448);
    // The flinch lasts until its animation ends (state 6 waits for the
    // script's end flag): a warrior's hit animation, 8, is four frames of four
    // ticks.
    for (int t = 0; t < 10; ++t) { EN::Tick(w, 300, 0, 0); }
    CHECK(!EN::ApplyHit(w, 32));
    CHECK_EQ(w.health, 448);
    for (int t = 0; t < 20; ++t) { EN::Tick(w, 300, 0, 0); }
    EN::ApplyHit(w, 32);
    CHECK_EQ(w.health, 416);                 // it used to stay at 448 for good
}

TEST_CASE("Animation scripts: a guard's attack fires four rounds, the others one")
{
    namespace SA = ALTEngine::Formats::SpriteAnim;
    namespace AS = ALTEngine::Formats::EnemyAnimSequences;
    namespace AD = ALTEngine::Formats::EnemyAnimData;
    auto shots = [](int type, int animation) {
        const auto seq = AS::Find(AD::CreatureIndexForType(type), animation);
        const SA::SequenceView view{ seq.data, seq.count };
        SA::Animator a;
        SA::Start(a, view);
        int fired = 0;
        for (int t = 0; t < 400 && !a.Ended(); ++t)
        {
            SA::Tick(a, view);
            if (a.flags & SA::FLAG_USER8) { ++fired; a.flags = static_cast<uint8_t>(a.flags & ~SA::FLAG_USER8); }
        }
        CHECK(a.Ended());
        return fired;
    };
    CHECK_EQ(shots(0x0b, 9), 4);    // guard: its burst loops three more times
    CHECK_EQ(shots(0x0c, 9), 1);    // soldier
    CHECK_EQ(shots(0x0d, 16), 1);   // synth
    CHECK_EQ(shots(0x0e, 11), 1);   // handler's shot
    CHECK_EQ(shots(0x0e, 10), 1);   // handler's throw
}

TEST_CASE("Animation scripts: the walk is chosen by view, and views 5-7 mirror")
{
    EN::Enemy h;
    h.type = 2; h.active = true; h.alive = true;
    h.facing = 0;
    // Facing the player head on: the front view, 4.
    EN::PlayWalkByView(h, 0);
    CHECK_EQ(h.animation, 4);
    CHECK(!h.mirrored);
    // Turned a quarter away: a side view, and one side is mirrored.
    h.facing = 0x400;
    EN::PlayWalkByView(h, 0);
    CHECK_EQ(h.animation, 6);
    CHECK(h.mirrored);
    // The script drives the frame: a hugger's walk holds each frame 4 ticks.
    CHECK_EQ(h.sequence.size() > 0, true);
    CHECK_EQ(h.animator.frameDuration, 4);
}

TEST_CASE("Animation scripts: a handler throws a grenade five times in eight in the middle band")
{
    bool arm = false;
    CHECK_EQ(EN::ActAnimation(0x0e, 0, 7, arm), 11);
    CHECK(!arm);
    CHECK_EQ(EN::ActAnimation(0x0e, 2, 2, arm), 11);
    CHECK(!arm);
    CHECK_EQ(EN::ActAnimation(0x0e, 2, 3, arm), 10);
    CHECK(arm);
    CHECK_EQ(EN::ActAnimation(6, 0, 0, arm), 11);   // a warrior's swipe
    CHECK_EQ(EN::ActAnimation(6, 2, 0, arm), 12);   // and its middle-band attack
}

TEST_CASE("Damage: a creature's hit lands from the near and middle bands with a clear line")
{
    namespace EB = ALTEngine::Screens::EnemyBehaviour;
    EN::Enemy w;
    w.type = 6; w.zoneVisible = true; w.coneHalfWidth = 0x200;
    w.x = 0; w.z = 0;
    w.facing = EN::BearingTo(w, 0, -1000);
    w.mode = 0; w.lineOfFire = false;
    CHECK_EQ(EN::ResolveAttack(w, 0, -1000), EB::AttackDamage(6, 0));   // contact: no line needed
    w.mode = 1;
    CHECK_EQ(EN::ResolveAttack(w, 0, -1000), 0);
    w.lineOfFire = true;
    CHECK_EQ(EN::ResolveAttack(w, 0, -1000), 15);
    w.mode = 2;
    CHECK_EQ(EN::ResolveAttack(w, 0, -1000), 10);
    w.mode = 3;
    CHECK_EQ(EN::ResolveAttack(w, 0, -1000), 0);
    w.mode = 0; w.zoneVisible = false;
    CHECK_EQ(EN::ResolveAttack(w, 0, -1000), 0);
}

#include "Formats/ExplosionGraphics.h"

TEST_CASE("Vents: each jet's sprite is its own frame and size for its life")
{
    namespace VS = ALTEngine::Formats::ExplosionGraphics::VentSprites;
    // A fresh steam jet (life 0x20, clamped to 0x1f) on its first frame,
    // wide and flat; an old one swells on frame 8.
    VS::Look young = VS::For(true, false, 0x20, 0);
    CHECK_EQ(young.record, 4);
    CHECK(std::fabs(young.halfWidth - 8 * 15000.0f / 256) < 0.01f);
    CHECK(std::fabs(young.halfHeight - 2 * 15000.0f / 256) < 0.01f);
    VS::Look old = VS::For(true, false, 0, 3);
    CHECK_EQ(old.record, 8);
    CHECK(old.halfHeight > young.halfHeight);
    // Vertical steam is the same turned on end.
    VS::Look up = VS::For(true, true, 0x20, 0);
    CHECK(up.halfWidth == young.halfHeight && up.halfHeight == young.halfWidth);
    // A flame starts on frame 2 and burns down to 7.
    CHECK_EQ(VS::For(false, false, 0x1f, 0).record, 2);
    CHECK_EQ(VS::For(false, false, 0, 3).record, 7);
    CHECK(std::fabs(VS::For(false, false, 0, 0).grey - 0x69 / 128.0f) < 0.001f);
}
