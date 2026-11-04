#include "TestFramework.h"

#include "Net/Protocol.h"
#include "Net/Session.h"

#include <chrono>
#include <cmath>
#include <thread>

namespace N = ALTEngine::Net;

namespace
{
    template <class T> T RoundTrip(const T& message)
    {
        const auto decoded = N::Decode(N::Encode(message));
        REQUIRE(decoded.has_value());
        REQUIRE(std::holds_alternative<T>(*decoded));
        return std::get<T>(*decoded);
    }

    // Runs every session's Update a few times, a little apart, so datagrams land.
    void Pump(std::initializer_list<N::Session*> sessions, double& now, int rounds = 20)
    {
        for (int i = 0; i < rounds; ++i)
        {
            now += 0.05;
            for (auto* s : sessions) { s->Update(now); }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

TEST_CASE("Net: every message survives encoding")
{
    N::State s; s.slot = 2; s.tick = 123456; s.x = -1234.5f; s.y = 96.0f; s.z = 777.25f;
    s.yaw = 0xabc; s.pitch = -0x40; s.health = 87; s.weapon = 3; s.flags = 5;
    const auto s2 = RoundTrip(s);
    CHECK(s2.slot == 2); CHECK(s2.tick == 123456u); CHECK(s2.x == -1234.5f); CHECK(s2.z == 777.25f);
    CHECK(s2.yaw == 0xabc); CHECK(s2.pitch == -0x40); CHECK(s2.health == 87); CHECK(s2.weapon == 3); CHECK(s2.flags == 5);

    N::Roster r; r.players[0] = { true, "Ripley", 3, N::Skin::Guard }; r.players[2] = { true, "Hicks", -1 };
    const auto r2 = RoundTrip(r);
    CHECK(r2.players[0].used); CHECK(r2.players[0].name == "Ripley"); CHECK(r2.players[0].frags == 3);
    CHECK(!r2.players[1].used); CHECK(r2.players[2].frags == -1);
    CHECK(r2.players[0].skin == N::Skin::Guard); CHECK(r2.players[2].skin == N::Skin::Synthetic);
    CHECK(RoundTrip(N::Join{ "Bishop", N::Skin::Guard }).skin == N::Skin::Guard);
    CHECK(N::SkinCreatureType(N::Skin::Synthetic) == 0x0d);
    CHECK(N::SkinCreatureType(N::Skin::Soldier) == 0x0c);
    CHECK(N::SkinCreatureType(N::Skin::Guard) == 0x0b);
    CHECK(N::SkinCreatureType(N::Skin::Handler) == 0x0e);
    // A look this version doesn't know arrives as the Synthetic.
    auto bytes = N::Encode(N::Join{ "X", N::Skin::Handler });
    bytes.back() = 9;
    CHECK(std::get<N::Join>(*N::Decode(bytes)).skin == N::Skin::Synthetic);

    N::Shot shot; shot.slot = 1; shot.type = 2; shot.vx = 256.0f; shot.vz = -12.5f;
    CHECK(RoundTrip(shot).vz == -12.5f);
    CHECK(RoundTrip(N::Hit{ 1, 3, 0x40 }).damage == 0x40);
    CHECK(RoundTrip(N::Frag{ 2, 1 }).victim == 1);
    CHECK(RoundTrip(N::Chat{ 1, "Get away from her" }).text == "Get away from her");
    CHECK(RoundTrip(N::DiscoverReply{ "LV426", "900", 2, 4 }).level == "900");
    CHECK(RoundTrip(N::Welcome{ 3, "901" }).slot == 3);
    // Names are cut to their limit.
    CHECK(RoundTrip(N::Join{ std::string(40, 'x') }).name.size() == N::MAX_NAME);
}

TEST_CASE("Net: anything not a whole message of ours is dropped")
{
    auto bytes = N::Encode(N::State{});
    for (size_t n = 0; n < bytes.size(); ++n)
    {
        CHECK(!N::Decode(std::vector<uint8_t>(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(n))).has_value());
    }
    auto wrong = bytes; wrong[0] = 'X';
    CHECK(!N::Decode(wrong).has_value());
    auto version = bytes; version[3] = N::PROTOCOL_VERSION + 1;
    CHECK(!N::Decode(version).has_value());
    auto type = bytes; type[4] = 0xee;
    CHECK(!N::Decode(type).has_value());
}

TEST_CASE("Net: a host and two players on this machine")
{
    N::Session host, a, b;
    REQUIRE(host.StartHost("Test", "900", "Ripley", 0));
    const auto address = N::Address::Loopback(host.Port());
    REQUIRE(a.StartJoin(address, "Hicks", N::Skin::Guard));
    REQUIRE(b.StartJoin(address, "Hudson"));
    double now = 0.0;
    Pump({ &host, &a, &b }, now);
    REQUIRE(a.Connected());
    REQUIRE(b.Connected());
    CHECK(a.Level() == "900");
    CHECK(a.LocalSlot() != b.LocalSlot());
    CHECK(a.LocalSlot() >= 1);
    CHECK(host.GetRoster().players[static_cast<size_t>(a.LocalSlot())].name == "Hicks");
    CHECK(b.GetRoster().players[0].name == "Ripley");
    CHECK(b.GetRoster().players[static_cast<size_t>(a.LocalSlot())].skin == N::Skin::Guard);
    host.TakeEvents(); a.TakeEvents(); b.TakeEvents();

    // A player's state reaches the host and, through it, the other player.
    N::State s; s.x = 100.0f; s.z = 200.0f; s.health = 90;
    a.SendState(s);
    N::State h; h.x = -50.0f;
    host.SendState(h);
    Pump({ &host, &a, &b }, now);
    const auto ia = static_cast<size_t>(a.LocalSlot());
    REQUIRE(host.RemoteStates()[ia].has_value());
    CHECK(host.RemoteStates()[ia]->x == 100.0f);
    REQUIRE(b.RemoteStates()[ia].has_value());
    CHECK(b.RemoteStates()[ia]->health == 90);
    REQUIRE(a.RemoteStates()[0].has_value());
    CHECK(a.RemoteStates()[0]->x == -50.0f);
    CHECK(!a.RemoteStates()[ia].has_value());   // never our own

    // A hit goes to the player hit, and only them.
    a.SendHit(static_cast<uint8_t>(b.LocalSlot()), 0x30);
    Pump({ &host, &a, &b }, now);
    bool bHit = false;
    for (const auto& e : b.TakeEvents()) { if (e.kind == N::Event::Kind::Hit) { bHit = true; CHECK(e.hit.damage == 0x30); CHECK(e.hit.attacker == a.LocalSlot()); } }
    CHECK(bHit);
    for (const auto& e : a.TakeEvents()) { CHECK(e.kind != N::Event::Kind::Hit); }

    // The player killed reports it; the killer scores, everyone hears.
    b.SendFrag(static_cast<uint8_t>(a.LocalSlot()), static_cast<uint8_t>(b.LocalSlot()));
    Pump({ &host, &a, &b }, now);
    CHECK(host.GetRoster().players[ia].frags == 1);
    CHECK(a.GetRoster().players[ia].frags == 1);
    bool aSawFrag = false;
    for (const auto& e : a.TakeEvents()) { if (e.kind == N::Event::Kind::Frag) { aSawFrag = true; } }
    CHECK(aSawFrag);
    // Killing yourself costs one.
    b.SendFrag(static_cast<uint8_t>(b.LocalSlot()), static_cast<uint8_t>(b.LocalSlot()));
    Pump({ &host, &a, &b }, now);
    CHECK(host.GetRoster().players[static_cast<size_t>(b.LocalSlot())].frags == -1);

    // Leaving frees the slot for everyone.
    const auto ib = static_cast<size_t>(b.LocalSlot());
    b.Stop();
    Pump({ &host, &a }, now);
    CHECK(!host.GetRoster().players[ib].used);
    CHECK(!a.GetRoster().players[ib].used);
    CHECK(!a.RemoteStates()[ib].has_value());
}

TEST_CASE("Net: a fifth player is turned away, and a silent one times out")
{
    N::Session host, p1, p2, p3, p4;
    REQUIRE(host.StartHost("Full", "901", "Host", 0));
    const auto address = N::Address::Loopback(host.Port());
    for (N::Session* p : { &p1, &p2, &p3, &p4 }) { REQUIRE(p->StartJoin(address, "P")); }
    double now = 0.0;
    Pump({ &host, &p1, &p2, &p3, &p4 }, now);
    int connected = 0;
    for (N::Session* p : { &p1, &p2, &p3, &p4 }) { connected += p->Connected() ? 1 : 0; }
    CHECK(connected == 3);
    bool rejected = false;
    for (N::Session* p : { &p1, &p2, &p3, &p4 })
    {
        for (const auto& e : p->TakeEvents()) { if (e.kind == N::Event::Kind::Rejected) { rejected = true; CHECK(e.reason == N::RejectReason::Full); } }
    }
    CHECK(rejected);

    // p1 goes quiet: after the timeout the host lets the slot go.
    const auto i1 = static_cast<size_t>(p1.LocalSlot());
    REQUIRE(p1.Connected());
    host.Update(now);   // read what is already waiting
    now += N::Session::TIMEOUT_SECONDS + 1.0;
    host.Update(now);
    CHECK(!host.GetRoster().players[i1].used);
}

TEST_CASE("Net: bots hold places up to the player limit and give them up to players")
{
    N::Session host, p1, p2;
    REQUIRE(host.StartHost("Bots", "900", "Host", 0));
    host.SetPlayerLimit(5);
    CHECK(host.PlayerLimit() == 5);
    host.SetPlayerLimit(12);
    CHECK(host.PlayerLimit() == N::MAX_PLAYERS);
    host.SetPlayerLimit(5);
    const int b1 = host.AddBot("BOT 1", N::Skin::Synthetic);
    const int b2 = host.AddBot("BOT 2", N::Skin::Soldier);
    const int b3 = host.AddBot("BOT 3", N::Skin::Guard);
    CHECK(b1 >= 1); CHECK(b2 >= 1); CHECK(b3 >= 1);
    CHECK(host.IsBot(b1));
    CHECK(!host.IsBot(0));
    CHECK(host.BotCount() == 3);
    CHECK(host.UsedCount() == 4);

    const auto address = N::Address::Loopback(host.Port());
    REQUIRE(p1.StartJoin(address, "Hicks"));
    double now = 0.0;
    Pump({ &host, &p1 }, now);
    REQUIRE(p1.Connected());
    CHECK(host.BotCount() == 3);   // room for one more
    CHECK(p1.GetRoster().players[static_cast<size_t>(b1)].bot);
    CHECK(!p1.GetRoster().players[0].bot);

    REQUIRE(p2.StartJoin(address, "Hudson"));
    Pump({ &host, &p1, &p2 }, now);
    REQUIRE(p2.Connected());       // a bot made way
    CHECK(host.BotCount() == 2);
    CHECK(host.UsedCount() == 5);
    host.TakeEvents(); p1.TakeEvents(); p2.TakeEvents();

    // A player's round on a bot is the host's to apply.
    const int bot = host.IsBot(b1) ? b1 : b2;
    p1.SendHit(static_cast<uint8_t>(bot), 40);
    Pump({ &host, &p1, &p2 }, now);
    bool botHit = false;
    for (const auto& e : host.TakeEvents())
    {
        if (e.kind == N::Event::Kind::BotHit)
        {
            botHit = true;
            CHECK(e.hit.target == bot);
            CHECK(e.hit.attacker == p1.LocalSlot());
            CHECK(e.hit.damage == 40);
        }
    }
    CHECK(botHit);

    // A bot's round on a player reaches that player as a hit from the bot.
    host.SendBotHit(bot, static_cast<uint8_t>(p2.LocalSlot()), 24);
    Pump({ &host, &p1, &p2 }, now);
    bool hit = false;
    for (const auto& e : p2.TakeEvents())
    {
        if (e.kind == N::Event::Kind::Hit) { hit = true; CHECK(e.hit.attacker == bot); CHECK(e.hit.damage == 24); }
    }
    CHECK(hit);

    // Its state reaches the players as a player's would.
    N::State s; s.x = 321.0f; s.health = 77;
    host.SendBotState(bot, s);
    Pump({ &host, &p1, &p2 }, now);
    REQUIRE(p1.RemoteStates()[static_cast<size_t>(bot)].has_value());
    CHECK(p1.RemoteStates()[static_cast<size_t>(bot)]->x == 321.0f);

    host.RemoveBot(bot);
    Pump({ &host, &p1, &p2 }, now);
    CHECK(!host.IsBot(bot));
    CHECK(!p1.GetRoster().players[static_cast<size_t>(bot)].used);
    CHECK(!p1.RemoteStates()[static_cast<size_t>(bot)].has_value());
}

TEST_CASE("Net: the game browser finds a game on this machine")
{
    N::Session host;
    REQUIRE(host.StartHost("Hadley's Hope", "905", "Host", 0));
    N::GameBrowser browser;
    REQUIRE(browser.Start(host.Port()));
    double now = 0.0;
    for (int i = 0; i < 20 && browser.Games().empty(); ++i)
    {
        now += 0.05;
        browser.Update(now);
        host.Update(now);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        browser.Update(now);
    }
    REQUIRE(browser.Games().size() == 1);
    CHECK(browser.Games()[0].info.gameName == "Hadley's Hope");
    CHECK(browser.Games()[0].info.level == "905");
    CHECK(browser.Games()[0].info.players == 1);
}

TEST_CASE("Net: a message with a slot past the roster, a non-number or a foreign round is refused")
{
    N::State s; s.slot = 1; s.x = 10.0f;
    CHECK(N::Decode(N::Encode(s)).has_value());
    s.slot = N::MAX_PLAYERS;
    CHECK(!N::Decode(N::Encode(s)).has_value());
    s.slot = 1; s.x = std::nanf("");
    CHECK(!N::Decode(N::Encode(s)).has_value());
    s.x = 1.0e9f;
    CHECK(!N::Decode(N::Encode(s)).has_value());

    N::Shot shot; shot.slot = 2; shot.type = 2;
    CHECK(N::Decode(N::Encode(shot)).has_value());
    shot.type = 9;   // acid blood
    CHECK(!N::Decode(N::Encode(shot)).has_value());

    N::Welcome w; w.slot = 200; w.level = "900";
    CHECK(!N::Decode(N::Encode(w)).has_value());
    N::Hit h; h.attacker = 1; h.target = 9;
    CHECK(!N::Decode(N::Encode(h)).has_value());
}

TEST_CASE("Net: the game mode and the co-operative messages survive encoding")
{
    N::DiscoverReply reply{ "Nostromo", "111", 1, 4, N::GameMode::Coop };
    CHECK(RoundTrip(reply).mode == N::GameMode::Coop);
    const auto w = RoundTrip(N::Welcome{ 2, "131", N::GameMode::Coop, 7 });
    CHECK(w.mode == N::GameMode::Coop); CHECK(w.levelSerial == 7); CHECK(w.level == "131");
    CHECK(RoundTrip(N::AtExit{ 3 }).slot == 3);
    CHECK(RoundTrip(N::LevelEnd{ 9 }).levelSerial == 9);
    const auto ls = RoundTrip(N::LevelStart{ 4, "141" });
    CHECK(ls.levelSerial == 4); CHECK(ls.level == "141");
    CHECK(RoundTrip(N::LevelStart{ 5, "" }).level.empty());
    N::Roster r; r.players[1] = { true, "Hicks", 0, N::Skin::Soldier, false, true };
    CHECK(RoundTrip(r).players[1].atExit);
    CHECK(!RoundTrip(r).players[0].atExit);
    // A mode this version doesn't know is refused.
    auto bytes = N::Encode(N::Welcome{ 1, "111", N::GameMode::Coop, 0 });
    bytes[bytes.size() - 3] = 7;
    CHECK(!N::Decode(bytes).has_value());
}

TEST_CASE("Net: co-operative - the level ends only once every player is at the exit")
{
    N::Session host, a, b;
    REQUIRE(host.StartHost("Coop", "111", "Ripley", 0));
    host.SetGameMode(N::GameMode::Coop);
    const auto address = N::Address::Loopback(host.Port());
    REQUIRE(a.StartJoin(address, "Hicks"));
    REQUIRE(b.StartJoin(address, "Hudson"));
    double now = 0.0;
    Pump({ &host, &a, &b }, now);
    REQUIRE(a.Connected()); REQUIRE(b.Connected());
    CHECK(a.Coop()); CHECK(b.Mode() == N::GameMode::Coop);
    CHECK(host.PlayersPlaying() == 3);
    host.TakeEvents(); a.TakeEvents(); b.TakeEvents();

    auto ended = [](N::Session& s) {
        for (const auto& e : s.TakeEvents()) { if (e.kind == N::Event::Kind::LevelEnded) { return true; } }
        return false;
    };

    // Two of three at the exit: the level goes on.
    a.ReportAtExit();
    host.ReportAtExit();
    Pump({ &host, &a, &b }, now);
    CHECK(host.PlayersAtExit() == 2);
    CHECK(b.PlayersAtExit() == 2);
    CHECK(!host.LevelOver());
    CHECK(!ended(host)); CHECK(!ended(a)); CHECK(!ended(b));

    // The last one: it ends everywhere.
    b.ReportAtExit();
    Pump({ &host, &a, &b }, now);
    CHECK(host.LevelOver()); CHECK(a.LevelOver()); CHECK(b.LevelOver());
    CHECK(ended(host)); CHECK(ended(a)); CHECK(ended(b));

    // The host moves on; the guests follow, with nobody at the exit.
    const uint16_t serial = a.LevelSerial();
    host.AdvanceLevel("112");
    Pump({ &host, &a, &b }, now);
    CHECK(a.Level() == "112"); CHECK(b.Level() == "112");
    CHECK(a.LevelSerial() == serial + 1);
    CHECK(!a.LevelOver()); CHECK(!a.AtExitReported());
    CHECK(host.PlayersAtExit() == 0); CHECK(a.PlayersAtExit() == 0);
    bool started = false;
    for (const auto& e : a.TakeEvents()) { started |= e.kind == N::Event::Kind::LevelStarted; }
    CHECK(started);
}

TEST_CASE("Net: co-operative - a player who leaves is not waited for")
{
    N::Session host, a, b;
    REQUIRE(host.StartHost("Coop", "111", "Ripley", 0));
    host.SetGameMode(N::GameMode::Coop);
    const auto address = N::Address::Loopback(host.Port());
    REQUIRE(a.StartJoin(address, "Hicks"));
    REQUIRE(b.StartJoin(address, "Hudson"));
    double now = 0.0;
    Pump({ &host, &a, &b }, now);
    host.ReportAtExit();
    a.ReportAtExit();
    Pump({ &host, &a, &b }, now);
    CHECK(!host.LevelOver());
    b.Stop();
    Pump({ &host, &a }, now);
    CHECK(host.LevelOver());
    CHECK(a.LevelOver());
}

TEST_CASE("Net: deathmatch has no level end to wait for")
{
    N::Session host, a;
    REQUIRE(host.StartHost("DM", "900", "Ripley", 0));
    REQUIRE(a.StartJoin(N::Address::Loopback(host.Port()), "Hicks"));
    double now = 0.0;
    Pump({ &host, &a }, now);
    CHECK(!a.Coop());
    host.ReportAtExit();
    a.ReportAtExit();
    Pump({ &host, &a }, now);
    CHECK(!host.LevelOver());
    CHECK(host.PlayersAtExit() == 0);
}

TEST_CASE("Net: co-operative world events reach every machine, in one order, and a late joiner catches up")
{
    N::Session host, a, b;
    REQUIRE(host.StartHost("Coop", "111", "Ripley", 0));
    host.SetGameMode(N::GameMode::Coop);
    const auto address = N::Address::Loopback(host.Port());
    REQUIRE(a.StartJoin(address, "Hicks"));
    double now = 0.0;
    Pump({ &host, &a }, now);
    REQUIRE(a.Connected());
    host.TakeEvents(); a.TakeEvents();

    auto worldOf = [](N::Session& s) {
        std::vector<N::WorldEvent> out;
        for (const auto& e : s.TakeEvents()) { if (e.kind == N::Event::Kind::World) { out.push_back(e.world); } }
        return out;
    };

    // The host's own go straight out; a guest's go through the host.
    N::WorldEvent door; door.kind = N::WorldKind::Trigger; door.a = 7; door.b = 3; door.flags = 0x08;
    host.SendWorldEvent(door);
    N::WorldEvent pickup; pickup.kind = N::WorldKind::PickupTaken; pickup.a = 12;
    a.SendWorldEvent(pickup);
    Pump({ &host, &a }, now);
    const auto atHost = worldOf(host);
    REQUIRE(atHost.size() == 1);
    CHECK(atHost[0].kind == N::WorldKind::PickupTaken);
    CHECK(atHost[0].a == 12);
    CHECK(atHost[0].origin == a.LocalSlot());
    const auto atA = worldOf(a);
    REQUIRE(atA.size() == 1);            // its own is not handed back
    CHECK(atA[0].kind == N::WorldKind::Trigger);
    CHECK(atA[0].a == 7); CHECK(atA[0].b == 3); CHECK(atA[0].flags == 0x08);

    // A player joining now is sent both, in the host's order.
    REQUIRE(b.StartJoin(address, "Hudson"));
    Pump({ &host, &a, &b }, now, 40);
    REQUIRE(b.Connected());
    const auto atB = worldOf(b);
    REQUIRE(atB.size() == 2);
    CHECK(atB[0].kind == N::WorldKind::Trigger);
    CHECK(atB[1].kind == N::WorldKind::PickupTaken);
    CHECK(atB[1].origin == a.LocalSlot());
    CHECK(worldOf(a).empty());          // nothing again for the others

    // A new level starts a new list.
    host.AdvanceLevel("112");
    Pump({ &host, &a, &b }, now);
    N::WorldEvent second; second.kind = N::WorldKind::ObjectDestroyed; second.a = 4;
    b.SendWorldEvent(second);
    Pump({ &host, &a, &b }, now);
    const auto atA2 = worldOf(a);
    REQUIRE(atA2.size() == 1);
    CHECK(atA2[0].kind == N::WorldKind::ObjectDestroyed);
    CHECK(atA2[0].seq == 1);
}

TEST_CASE("Net: co-operative creatures - the host's state out, a guest's hits in, a creature's hits and rounds")
{
    N::Session host, a;
    REQUIRE(host.StartHost("Coop", "111", "Ripley", 0));
    host.SetGameMode(N::GameMode::Coop);
    REQUIRE(a.StartJoin(N::Address::Loopback(host.Port()), "Hicks"));
    double now = 0.0;
    Pump({ &host, &a }, now);
    REQUIRE(a.Connected());
    CHECK(a.CoopGuest()); CHECK(host.CoopHost());
    host.TakeEvents(); a.TakeEvents();

    // More than one message's worth.
    std::vector<N::CreatureEntry> entries;
    for (int i = 0; i < 60; ++i)
    {
        N::CreatureEntry c; c.index = static_cast<uint16_t>(i); c.flags = N::CREATURE_ACTIVE | N::CREATURE_ALIVE;
        c.x = static_cast<float>(i * 10); c.animation = 3; c.frame = 2; c.health = 40; c.sound = i == 5 ? 0x2c : 0xff;
        entries.push_back(c);
    }
    host.SendCreatures(entries);
    a.SendCreatureDamage(17, 0x40);
    host.SendCreatureHit(static_cast<uint8_t>(a.LocalSlot()), 9, 2, true, 0x400);
    N::CreatureShot shot; shot.kind = 1; shot.x = 5; shot.vz = -12;
    host.SendCreatureShot(shot);
    Pump({ &host, &a }, now);

    size_t seen = 0;
    bool sawSound = false, sawHit = false, sawShot = false;
    for (const auto& e : a.TakeEvents())
    {
        if (e.kind == N::Event::Kind::Creatures)
        {
            for (const auto& c : e.creatures.entries)
            {
                seen++;
                if (c.index == 5) { sawSound = c.sound == 0x2c; CHECK(c.x == 50.0f); }
            }
        }
        if (e.kind == N::Event::Kind::CreatureHit)
        {
            sawHit = true;
            CHECK(e.creatureHit.damage == 9); CHECK(e.creatureHit.cause == 2); CHECK(e.creatureHit.knock == 1);
            CHECK(e.creatureHit.yaw == 0x400);
        }
        if (e.kind == N::Event::Kind::CreatureShot) { sawShot = true; CHECK(e.creatureShot.kind == 1); CHECK(e.creatureShot.vz == -12.0f); }
    }
    CHECK(seen == 60);
    CHECK(sawSound); CHECK(sawHit); CHECK(sawShot);

    bool sawDamage = false;
    for (const auto& e : host.TakeEvents())
    {
        if (e.kind != N::Event::Kind::CreatureDamage) { continue; }
        sawDamage = true;
        CHECK(e.creatureDamage.index == 17); CHECK(e.creatureDamage.damage == 0x40);
        CHECK(e.creatureDamage.slot == a.LocalSlot());
    }
    CHECK(sawDamage);

    // A deathmatch has none of it.
    N::Session dm;
    REQUIRE(dm.StartHost("DM", "900", "R", 0));
    CHECK(!dm.CoopHost());
}
