#include "TestFramework.h"

#include "Game/Bots.h"

namespace B = ALTEngine::Game::Bots;

namespace
{
    // An open floor: every step goes where it is asked.
    B::World OpenWorld(bool clear)
    {
        B::World w;
        w.clearShot = [clear](float, float, float, float, float, float) { return clear; };
        w.move = [](float& x, float& z, float, float sx, float sz) { x += sx; z += sz; };
        w.floor = [](float, float) { return 0.0f; };
        return w;
    }
}

TEST_CASE("Bots: skill runs from slow and loose to quick and close")
{
    const B::Skill easy = B::SkillFor(1), hard = B::SkillFor(5);
    CHECK(easy.reactionTicks > hard.reactionTicks);
    CHECK(easy.aimSpread > hard.aimSpread);
    CHECK(easy.fireInterval > hard.fireInterval);
    CHECK(hard.walkSpeed <= B::PLAYER_WALK);
    CHECK(B::SkillFor(0).reactionTicks == easy.reactionTicks);   // clamped
    CHECK(B::SkillFor(9).reactionTicks == hard.reactionTicks);
}

TEST_CASE("Bots: angles follow the game's own (x += sin, z -= cos)")
{
    CHECK(B::AngleTo(0, 0, 0, -100) == 0);
    CHECK(B::AngleTo(0, 0, 100, 0) == 0x400);
    CHECK(B::AngleTo(0, 0, 0, 100) == 0x800);
    CHECK(B::AngleDelta(0xf00, 0x100) == 0x200);
    CHECK(B::TurnToward(0, 0x400, 0x100) == 0x100);
    CHECK(B::TurnToward(0, 0xc00, 0x100) == 0xf00);
}

TEST_CASE("Bots: a bot in view of a player turns, waits its reaction time, then fires")
{
    B::Bot bot; bot.slot = 1; bot.x = 0; bot.z = 0; bot.yaw = 0x800;
    const B::Skill skill = B::SkillFor(3);
    const auto world = OpenWorld(true);
    const std::vector<B::Target> targets{ { 0, 0.0f, 0.0f, -0x1000, true }, { 1, 0.0f, 0.0f, 0.0f, true } };
    int firedAt = -1;
    for (int t = 0; t < 200 && firedAt < 0; ++t)
    {
        if (B::Tick(bot, skill, world, targets).fire) { firedAt = t; }
    }
    CHECK(firedAt >= skill.reactionTicks - 1);
    CHECK(bot.targetSlot == 0);
    CHECK(std::abs(B::AngleDelta(bot.yaw, B::AngleTo(bot.x, bot.z, 0.0f, -0x1000))) < 0x100);
    CHECK(bot.z < 0.0f);                   // walked toward them
}

TEST_CASE("Bots: no clear line, no shot; dead players are not hunted")
{
    B::Bot bot; bot.slot = 2;
    const auto blocked = OpenWorld(false);
    const std::vector<B::Target> targets{ { 0, 0.0f, 0.0f, -0x800, true } };
    for (int t = 0; t < 200; ++t) { CHECK(!B::Tick(bot, B::SkillFor(5), blocked, targets).fire); }

    B::Bot other; other.slot = 3;
    const std::vector<B::Target> dead{ { 0, 0.0f, 0.0f, -0x800, false } };
    B::Tick(other, B::SkillFor(5), OpenWorld(true), dead);
    CHECK(other.targetSlot == -1);
    CHECK(!other.moving);
}

TEST_CASE("Bots: the way along the path network")
{
    // 0 - 1 - 2 - 3, and 1 - 4.
    std::vector<ALTEngine::Formats::PathNode> nodes(5);
    auto link = [&](int a, int b) {
        for (int k : { a, b })
        {
            auto& n = nodes[static_cast<size_t>(k)];
            const uint8_t other = static_cast<uint8_t>(k == a ? b : a);
            if (n.nodeA == 0xff) { n.nodeA = other; } else if (n.nodeB == 0xff) { n.nodeB = other; }
            else if (n.nodeC == 0xff) { n.nodeC = other; } else { n.nodeD = other; }
        }
    };
    for (auto& n : nodes) { n.nodeA = n.nodeB = n.nodeC = n.nodeD = 0xff; }
    link(0, 1); link(1, 2); link(2, 3); link(1, 4);
    CHECK(B::NextNode(nodes, 0, 3) == 1);
    CHECK(B::NextNode(nodes, 1, 3) == 2);
    CHECK(B::NextNode(nodes, 4, 3) == 1);
    CHECK(B::NextNode(nodes, 3, 3) == 3);
    CHECK(B::NextNode(nodes, 0, 9) == -1);
}

TEST_CASE("Bots: hits take a quarter, as a player's do, and a death waits to respawn")
{
    B::Bot bot;
    CHECK(!B::TakeHit(bot, 200, 4));
    CHECK(bot.health == B::HEALTH - 50);
    CHECK(bot.lastAttacker == 4);
    CHECK(B::TakeHit(bot, 400, 5));
    CHECK(bot.dead);
    CHECK(bot.respawnTicks == B::RESPAWN_TICKS);
    CHECK(!B::TakeHit(bot, 400, 5));   // already down
}
