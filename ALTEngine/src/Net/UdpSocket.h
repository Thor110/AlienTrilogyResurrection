#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ALTEngine::Net
{
    // An IPv4 address and port, both in host order.
    struct Address
    {
        uint32_t ip = 0;
        uint16_t port = 0;
        bool operator==(const Address& o) const { return ip == o.ip && port == o.port; }
        bool operator!=(const Address& o) const { return !(*this == o); }
        std::string ToString() const;
        static Address Loopback(uint16_t port) { return Address{ 0x7f000001u, port }; }
        static Address Broadcast(uint16_t port) { return Address{ 0xffffffffu, port }; }
    };

    // "1.2.3.4", "1.2.3.4:5000" or a host name; nullopt if it can't be read.
    std::optional<Address> Resolve(const std::string& text, uint16_t defaultPort);

    // A non-blocking UDP socket - the original's IPX packets, on today's
    // networks. Winsock on Windows, BSD sockets elsewhere.
    class UdpSocket
    {
    public:
        UdpSocket() = default;
        ~UdpSocket();
        UdpSocket(const UdpSocket&) = delete;
        UdpSocket& operator=(const UdpSocket&) = delete;

        // Binds to `port` on every interface (0 for any free port).
        bool Open(uint16_t port, bool allowBroadcast = false);
        void Close();
        bool IsOpen() const { return handle != INVALID; }
        uint16_t LocalPort() const { return localPort; }

        bool SendTo(const Address& to, const std::vector<uint8_t>& bytes);
        // One datagram, or nullopt when none is waiting.
        std::optional<std::vector<uint8_t>> Receive(Address& from);

    private:
        static constexpr intptr_t INVALID = -1;
        intptr_t handle = INVALID;
        uint16_t localPort = 0;
    };
}
