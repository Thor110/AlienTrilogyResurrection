#include "Protocol.h"

#include <cstring>

namespace ALTEngine::Net
{
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
            },
            [&](const Join& m) { w.U8(static_cast<uint8_t>(MessageType::Join)); w.Str(m.name, MAX_NAME); },
            [&](const Welcome& m) { w.U8(static_cast<uint8_t>(MessageType::Welcome)); w.U8(m.slot); w.Str(m.level, 8); },
            [&](const Reject& m) { w.U8(static_cast<uint8_t>(MessageType::Reject)); w.U8(static_cast<uint8_t>(m.reason)); },
            [&](const Roster& m) {
                w.U8(static_cast<uint8_t>(MessageType::Roster));
                for (const auto& p : m.players) { w.U8(p.used ? 1 : 0); w.Str(p.name, MAX_NAME); w.I16(p.frags); }
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
        }, message);
        return w.out;
    }

    std::optional<Message> Decode(const std::vector<uint8_t>& bytes)
    {
        if (bytes.size() < 5 || std::memcmp(bytes.data(), MAGIC, 3) != 0 || bytes[3] != PROTOCOL_VERSION) { return std::nullopt; }
        Reader r{ bytes, 5 };
        std::optional<Message> result;
        switch (static_cast<MessageType>(bytes[4]))
        {
        case MessageType::DiscoverQuery: result = DiscoverQuery{}; break;
        case MessageType::DiscoverReply:
        {
            DiscoverReply m; m.gameName = r.Str(MAX_GAME_NAME); m.level = r.Str(8); m.players = r.U8(); m.maxPlayers = r.U8();
            result = m; break;
        }
        case MessageType::Join: { Join m; m.name = r.Str(MAX_NAME); result = m; break; }
        case MessageType::Welcome: { Welcome m; m.slot = r.U8(); m.level = r.Str(8); result = m; break; }
        case MessageType::Reject: { Reject m; m.reason = static_cast<RejectReason>(r.U8()); result = m; break; }
        case MessageType::Roster:
        {
            Roster m;
            for (auto& p : m.players) { p.used = r.U8() != 0; p.name = r.Str(MAX_NAME); p.frags = r.I16(); }
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
        default: return std::nullopt;
        }
        if (!r.ok) { return std::nullopt; }
        return result;
    }
}
