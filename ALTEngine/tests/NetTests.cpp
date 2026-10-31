#include "TestFramework.h"

#include "Net/Protocol.h"
#include "Net/Session.h"

#include <chrono>
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
