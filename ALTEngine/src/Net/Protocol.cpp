#include "Protocol.h"

#include <cmath>
#include <cstring>

#ifndef ALTENGINE_VERSION
#define ALTENGINE_VERSION "unknown"
#endif

namespace ALTEngine::Net
{
    const char* GameVersion() { return ALTENGINE_VERSION; }

    namespace
    {
        constexpr uint8_t MAGIC[3] = { 'A', 'L', 'T' };

        struct Writer
        {
            std::vector<uint8_t> out;
            void U8(uint8_t v) { out.push_back(v); }
            void U16(uint16_t v) { U8(static_cast<uint8_t>(v)); U8(static_cast<uint8_t>(v >> 8)); }
            void I16(int16_t v) { U16(static_cast<uint16_t>(v)); }
            void U32(uint32_t v) { U16(static_cast<uint16_t>(v)); U16(static_cast<uint16_t>(v >> 16)); }
            void F32(float v) { uint32_t u; std::memcpy(&u, &v, 4); U32(u); }
            void Str(const std::string& s, size_t max)
            {
                const size_t n = s.size() < max ? s.size() : max;
                U8(static_cast<uint8_t>(n));
                out.insert(out.end(), s.begin(), s.begin() + static_cast<std::ptrdiff_t>(n));
            }
        };

        struct Reader
        {
            const std::vector<uint8_t>& in;
            size_t pos;
            bool ok = true;
            uint8_t U8() { if (pos >= in.size()) { ok = false; return 0; } return in[pos++]; }
            uint16_t U16() { const uint16_t lo = U8(); return static_cast<uint16_t>(lo | (U8() << 8)); }
            int16_t I16() { return static_cast<int16_t>(U16()); }
            uint32_t U32() { const uint32_t lo = U16(); return lo | (static_cast<uint32_t>(U16()) << 16); }
            float F32() { const uint32_t u = U32(); float v; std::memcpy(&v, &u, 4); return v; }
            std::string Str(size_t max)
            {
                const size_t n = U8();
                if (n > max || pos + n > in.size()) { ok = false; return {}; }
                std::string s(in.begin() + static_cast<std::ptrdiff_t>(pos), in.begin() + static_cast<std::ptrdiff_t>(pos + n));
                pos += n;
                return s;
            }
        };

        template <class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };
        template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

        bool Coordinate(float v) { return std::isfinite(v) && std::fabs(v) < MAX_COORDINATE; }
        bool Slot(uint8_t v) { return v < MAX_PLAYERS; }

        // What a peer sends goes straight into the game's arrays and its
        // cell lookups: a slot past the roster, a position that is not a
        // number (std::lround on it is undefined) or a round of a kind no
        // player fires is refused here rather than everywhere it is used.
        bool Sane(const Message& message)
        {
            return std::visit(Overloaded{
                [](const Welcome& m) { return Slot(m.slot) && m.mode < GameMode::Count; },
                [](const State& m) { return Slot(m.slot) && Coordinate(m.x) && Coordinate(m.y) && Coordinate(m.z); },
                [](const Shot& m) {
                    return Slot(m.slot) && m.type <= MAX_SHOT_TYPE && Coordinate(m.x) && Coordinate(m.y) && Coordinate(m.z)
                        && Coordinate(m.vx) && Coordinate(m.vy) && Coordinate(m.vz);
                },
                [](const Hit& m) { return Slot(m.attacker) && Slot(m.target); },
                [](const Frag& m) { return Slot(m.killer) && Slot(m.victim); },
                [](const Chat& m) { return Slot(m.slot); },
                [](const Leave& m) { return Slot(m.slot); },
                [](const Ping& m) { return Slot(m.slot); },
                [](const AtExit& m) { return Slot(m.slot); },
                [](const DiscoverReply& m) { return m.mode < GameMode::Count; },
                [](const LevelStart& m) { return m.level.size() <= 8; },
                [](const WorldEvent& m) {
                    return Slot(m.origin) && m.kind >= WorldKind::Trigger && m.kind < WorldKind::Count
                        && Coordinate(m.x) && Coordinate(m.y) && Coordinate(m.z)
                        && Coordinate(m.vx) && Coordinate(m.vy) && Coordinate(m.vz);
                },
                [](const Creatures& m) {
                    if (m.entries.size() > MAX_CREATURES_PER_MESSAGE) { return false; }
                    for (const auto& e : m.entries)
                    {
                        if (!Coordinate(e.x) || !Coordinate(e.y) || !Coordinate(e.z)) { return false; }
                    }
                    return true;
                },
                [](const CreatureDamage& m) { return Slot(m.slot); },
                [](const Throw& m) {
                    return Slot(m.slot) && m.charge <= 1 && Coordinate(m.x) && Coordinate(m.y) && Coordinate(m.z)
                        && Coordinate(m.vx) && Coordinate(m.vy) && Coordinate(m.vz);
                },
                [](const CreatureHit& m) { return Slot(m.target); },
                [](const CreatureShot& m) {
                    return m.kind <= 1 && Coordinate(m.x) && Coordinate(m.y) && Coordinate(m.z)
                        && Coordinate(m.vx) && Coordinate(m.vy) && Coordinate(m.vz);
                },
                [](const auto&) { return true; },
            }, message);
        }

        Skin SkinFrom(uint8_t v) { return v < static_cast<uint8_t>(Skin::Count) ? static_cast<Skin>(v) : Skin::Synthetic; }

    }

    std::vector<uint8_t> Encode(const Message& message)
    {
        Writer w;
        w.out.assign(MAGIC, MAGIC + 3);
        w.U8(PROTOCOL_VERSION);
        std::visit(Overloaded{
            [&](const DiscoverQuery&) { w.U8(static_cast<uint8_t>(MessageType::DiscoverQuery)); },
            [&](const DiscoverReply& m) {
                w.U8(static_cast<uint8_t>(MessageType::DiscoverReply));
                w.Str(m.gameName, MAX_GAME_NAME); w.Str(m.level, 8); w.U8(m.players); w.U8(m.maxPlayers);
                w.U8(static_cast<uint8_t>(m.mode));
            },
            [&](const Join& m) {
                w.U8(static_cast<uint8_t>(MessageType::Join)); w.Str(m.name, MAX_NAME); w.U8(static_cast<uint8_t>(m.skin));
                w.Str(m.gameVersion, MAX_VERSION_TEXT);
            },
            [&](const Welcome& m) {
                w.U8(static_cast<uint8_t>(MessageType::Welcome)); w.U8(m.slot); w.Str(m.level, 8);
                w.U8(static_cast<uint8_t>(m.mode)); w.U16(m.levelSerial);
                w.U32(m.rules.modernFeatures); w.U8(m.rules.weaponPacing);
            },
            [&](const Reject& m) {
                w.U8(static_cast<uint8_t>(MessageType::Reject)); w.U8(static_cast<uint8_t>(m.reason));
                w.Str(m.hostVersion, MAX_VERSION_TEXT);
            },
            [&](const Roster& m) {
                w.U8(static_cast<uint8_t>(MessageType::Roster));
                for (const auto& p : m.players)
                {
                    w.U8(static_cast<uint8_t>((p.used ? 1 : 0) | (p.bot ? 2 : 0) | (p.atExit ? 4 : 0)));
                    w.Str(p.name, MAX_NAME); w.I16(p.frags); w.U8(static_cast<uint8_t>(p.skin));
                }
            },
            [&](const State& m) {
                w.U8(static_cast<uint8_t>(MessageType::State));
                w.U8(m.slot); w.U32(m.tick); w.F32(m.x); w.F32(m.y); w.F32(m.z);
                w.I16(m.yaw); w.I16(m.pitch); w.I16(m.health); w.U8(m.weapon); w.U8(m.flags);
            },
            [&](const Shot& m) {
                w.U8(static_cast<uint8_t>(MessageType::Shot));
                w.U8(m.slot); w.U8(m.type); w.F32(m.x); w.F32(m.y); w.F32(m.z); w.F32(m.vx); w.F32(m.vy); w.F32(m.vz);
            },
            [&](const Hit& m) { w.U8(static_cast<uint8_t>(MessageType::Hit)); w.U8(m.attacker); w.U8(m.target); w.I16(m.damage); },
            [&](const Frag& m) { w.U8(static_cast<uint8_t>(MessageType::Frag)); w.U8(m.killer); w.U8(m.victim); },
            [&](const Chat& m) { w.U8(static_cast<uint8_t>(MessageType::Chat)); w.U8(m.slot); w.Str(m.text, MAX_CHAT); },
            [&](const Leave& m) { w.U8(static_cast<uint8_t>(MessageType::Leave)); w.U8(m.slot); },
            [&](const Ping& m) { w.U8(static_cast<uint8_t>(MessageType::Ping)); w.U8(m.slot); },
            [&](const AtExit& m) { w.U8(static_cast<uint8_t>(MessageType::AtExit)); w.U8(m.slot); },
            [&](const LevelEnd& m) { w.U8(static_cast<uint8_t>(MessageType::LevelEnd)); w.U16(m.levelSerial); },
            [&](const LevelStart& m) { w.U8(static_cast<uint8_t>(MessageType::LevelStart)); w.U16(m.levelSerial); w.Str(m.level, 8); },
            [&](const WorldEvent& m) {
                w.U8(static_cast<uint8_t>(MessageType::WorldEvent));
                w.U16(m.levelSerial); w.U32(m.seq); w.U8(m.origin); w.U8(static_cast<uint8_t>(m.kind));
                w.I16(m.a); w.I16(m.b); w.U8(m.flags);
                w.F32(m.x); w.F32(m.y); w.F32(m.z); w.F32(m.vx); w.F32(m.vy); w.F32(m.vz);
            },
            [&](const WorldAck& m) { w.U8(static_cast<uint8_t>(MessageType::WorldAck)); w.U16(m.levelSerial); w.U32(m.seq); },
            [&](const Creatures& m) {
                w.U8(static_cast<uint8_t>(MessageType::Creatures));
                w.U16(m.levelSerial);
                const size_t n = m.entries.size() < MAX_CREATURES_PER_MESSAGE ? m.entries.size() : MAX_CREATURES_PER_MESSAGE;
                w.U8(static_cast<uint8_t>(n));
                for (size_t i = 0; i < n; ++i)
                {
                    const auto& e = m.entries[i];
                    w.U16(e.index); w.U16(e.flags); w.F32(e.x); w.F32(e.y); w.F32(e.z);
                    w.U16(e.facing); w.I16(e.animation); w.U8(e.frame); w.U8(e.state); w.I16(e.health); w.U8(e.sound);
                }
            },
            [&](const CreatureDamage& m) {
                w.U8(static_cast<uint8_t>(MessageType::CreatureDamage));
                w.U16(m.levelSerial); w.U8(m.slot); w.U16(m.index); w.I16(m.damage);
            },
            [&](const CreatureHit& m) {
                w.U8(static_cast<uint8_t>(MessageType::CreatureHit));
                w.U8(m.target); w.I16(m.damage); w.U8(m.cause); w.U8(m.knock); w.I16(m.yaw);
            },
            [&](const Throw& m) {
                w.U8(static_cast<uint8_t>(MessageType::Throw));
                w.U8(m.slot); w.U8(m.charge);
                w.F32(m.x); w.F32(m.y); w.F32(m.z); w.F32(m.vx); w.F32(m.vy); w.F32(m.vz);
            },
            [&](const CreatureShot& m) {
                w.U8(static_cast<uint8_t>(MessageType::CreatureShot));
                w.U16(m.levelSerial); w.U8(m.kind);
                w.F32(m.x); w.F32(m.y); w.F32(m.z); w.F32(m.vx); w.F32(m.vy); w.F32(m.vz);
            },
        }, message);
        return w.out;
    }

    std::optional<Message> Decode(const std::vector<uint8_t>& bytes)
    {
        if (bytes.size() < 5 || std::memcmp(bytes.data(), MAGIC, 3) != 0) { return std::nullopt; }
        const uint8_t protocol = bytes[3];
        const auto type = static_cast<MessageType>(bytes[4]);
        // Another version's datagram: only the handshake is read.
        if (protocol != PROTOCOL_VERSION && type != MessageType::Join && type != MessageType::Reject) { return std::nullopt; }
        Reader r{ bytes, 5 };
        std::optional<Message> result;
        switch (static_cast<MessageType>(bytes[4]))
        {
        case MessageType::DiscoverQuery: result = DiscoverQuery{}; break;
        case MessageType::DiscoverReply:
        {
            DiscoverReply m; m.gameName = r.Str(MAX_GAME_NAME); m.level = r.Str(8); m.players = r.U8(); m.maxPlayers = r.U8();
            m.mode = static_cast<GameMode>(r.U8());
            result = m; break;
        }
        case MessageType::Join:
        {
            Join m; m.name = r.Str(MAX_NAME); m.skin = SkinFrom(r.U8());
            // Before 0.9.6 a Join ended there.
            m.gameVersion = r.pos < bytes.size() ? r.Str(MAX_VERSION_TEXT) : std::string();
            m.protocol = protocol;
            result = m; break;
        }
        case MessageType::Welcome:
        {
            Welcome m; m.slot = r.U8(); m.level = r.Str(8); m.mode = static_cast<GameMode>(r.U8()); m.levelSerial = r.U16();
            m.rules.modernFeatures = r.U32(); m.rules.weaponPacing = r.U8();
            result = m; break;
        }
        case MessageType::Reject:
        {
            Reject m; m.reason = static_cast<RejectReason>(r.U8());
            m.hostVersion = r.pos < bytes.size() ? r.Str(MAX_VERSION_TEXT) : std::string();
            result = m; break;
        }
        case MessageType::Roster:
        {
            Roster m;
            for (auto& p : m.players)
            {
                const uint8_t flags = r.U8();
                p.used = (flags & 1) != 0; p.bot = (flags & 2) != 0; p.atExit = (flags & 4) != 0;
                p.name = r.Str(MAX_NAME); p.frags = r.I16(); p.skin = SkinFrom(r.U8());
            }
            result = m; break;
        }
        case MessageType::State:
        {
            State m; m.slot = r.U8(); m.tick = r.U32(); m.x = r.F32(); m.y = r.F32(); m.z = r.F32();
            m.yaw = r.I16(); m.pitch = r.I16(); m.health = r.I16(); m.weapon = r.U8(); m.flags = r.U8();
            result = m; break;
        }
        case MessageType::Shot:
        {
            Shot m; m.slot = r.U8(); m.type = r.U8(); m.x = r.F32(); m.y = r.F32(); m.z = r.F32();
            m.vx = r.F32(); m.vy = r.F32(); m.vz = r.F32();
            result = m; break;
        }
        case MessageType::Hit: { Hit m; m.attacker = r.U8(); m.target = r.U8(); m.damage = r.I16(); result = m; break; }
        case MessageType::Frag: { Frag m; m.killer = r.U8(); m.victim = r.U8(); result = m; break; }
        case MessageType::Chat: { Chat m; m.slot = r.U8(); m.text = r.Str(MAX_CHAT); result = m; break; }
        case MessageType::Leave: { Leave m; m.slot = r.U8(); result = m; break; }
        case MessageType::Ping: { Ping m; m.slot = r.U8(); result = m; break; }
        case MessageType::AtExit: { AtExit m; m.slot = r.U8(); result = m; break; }
        case MessageType::LevelEnd: { LevelEnd m; m.levelSerial = r.U16(); result = m; break; }
        case MessageType::LevelStart: { LevelStart m; m.levelSerial = r.U16(); m.level = r.Str(8); result = m; break; }
        case MessageType::WorldEvent:
        {
            WorldEvent m; m.levelSerial = r.U16(); m.seq = r.U32(); m.origin = r.U8(); m.kind = static_cast<WorldKind>(r.U8());
            m.a = r.I16(); m.b = r.I16(); m.flags = r.U8();
            m.x = r.F32(); m.y = r.F32(); m.z = r.F32(); m.vx = r.F32(); m.vy = r.F32(); m.vz = r.F32();
            result = m; break;
        }
        case MessageType::WorldAck: { WorldAck m; m.levelSerial = r.U16(); m.seq = r.U32(); result = m; break; }
        case MessageType::Creatures:
        {
            Creatures m; m.levelSerial = r.U16();
            const size_t n = r.U8();
            if (n > MAX_CREATURES_PER_MESSAGE) { return std::nullopt; }
            m.entries.resize(n);
            for (auto& e : m.entries)
            {
                e.index = r.U16(); e.flags = r.U16(); e.x = r.F32(); e.y = r.F32(); e.z = r.F32();
                e.facing = r.U16(); e.animation = r.I16(); e.frame = r.U8(); e.state = r.U8(); e.health = r.I16(); e.sound = r.U8();
            }
            result = m; break;
        }
        case MessageType::CreatureDamage:
        {
            CreatureDamage m; m.levelSerial = r.U16(); m.slot = r.U8(); m.index = r.U16(); m.damage = r.I16();
            result = m; break;
        }
        case MessageType::CreatureHit:
        {
            CreatureHit m; m.target = r.U8(); m.damage = r.I16(); m.cause = r.U8(); m.knock = r.U8(); m.yaw = r.I16();
            result = m; break;
        }
        case MessageType::Throw:
        {
            Throw m; m.slot = r.U8(); m.charge = r.U8();
            m.x = r.F32(); m.y = r.F32(); m.z = r.F32(); m.vx = r.F32(); m.vy = r.F32(); m.vz = r.F32();
            result = m; break;
        }
        case MessageType::CreatureShot:
        {
            CreatureShot m; m.levelSerial = r.U16(); m.kind = r.U8();
            m.x = r.F32(); m.y = r.F32(); m.z = r.F32(); m.vx = r.F32(); m.vy = r.F32(); m.vz = r.F32();
            result = m; break;
        }
        default: return std::nullopt;
        }
        if (!r.ok || !result || !Sane(*result)) { return std::nullopt; }
        return result;
    }
}
