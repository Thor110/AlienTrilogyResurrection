#include "UdpSocket.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
   using SocketLength = int;
#else
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
   using SocketLength = socklen_t;
#endif

namespace ALTEngine::Net
{
    namespace
    {
#ifdef _WIN32
        // WSAStartup once for the life of the program.
        bool StartSockets()
        {
            static const bool started = [] {
                WSADATA data{};
                return WSAStartup(MAKEWORD(2, 2), &data) == 0;
            }();
            return started;
        }
        void CloseHandle(intptr_t h) { closesocket(static_cast<SOCKET>(h)); }
        using RawSocket = SOCKET;
#else
        bool StartSockets() { return true; }
        void CloseHandle(intptr_t h) { ::close(static_cast<int>(h)); }
        using RawSocket = int;
#endif
        sockaddr_in ToSockaddr(const Address& a)
        {
            sockaddr_in s{};
            s.sin_family = AF_INET;
            s.sin_addr.s_addr = htonl(a.ip);
            s.sin_port = htons(a.port);
            return s;
        }
    }

    std::string Address::ToString() const
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%u.%u.%u.%u:%u", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff,
                      ip & 0xff, static_cast<unsigned>(port));
        return text;
    }

    std::optional<Address> Resolve(const std::string& text, uint16_t defaultPort)
    {
        if (text.empty() || !StartSockets()) { return std::nullopt; }
        std::string host = text;
        uint16_t port = defaultPort;
        const size_t colon = text.rfind(':');
        if (colon != std::string::npos)
        {
            host = text.substr(0, colon);
            const std::string portText = text.substr(colon + 1);
            char* end = nullptr;
            const unsigned long value = std::strtoul(portText.c_str(), &end, 10);
            if (portText.empty() || *end != '\0' || value == 0 || value > 65535) { return std::nullopt; }
            port = static_cast<uint16_t>(value);
        }
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* found = nullptr;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &found) != 0 || !found) { return std::nullopt; }
        Address result;
        result.ip = ntohl(reinterpret_cast<sockaddr_in*>(found->ai_addr)->sin_addr.s_addr);
        result.port = port;
        freeaddrinfo(found);
        return result;
    }

    UdpSocket::~UdpSocket() { Close(); }

    bool UdpSocket::Open(uint16_t port, bool allowBroadcast)
    {
        Close();
        if (!StartSockets()) { return false; }
        const RawSocket s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
        if (s == INVALID_SOCKET) { return false; }
        u_long nonBlocking = 1;
        ioctlsocket(s, FIONBIO, &nonBlocking);
        // A datagram to a peer that has gone makes Windows report a reset on
        // the next receive; with this off, a lost player can't stall the loop.
#  ifndef SIO_UDP_CONNRESET
#    define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#  endif
        BOOL reportReset = FALSE;
        DWORD returned = 0;
        WSAIoctl(s, SIO_UDP_CONNRESET, &reportReset, sizeof(reportReset), nullptr, 0, &returned, nullptr, nullptr);
#else
        if (s < 0) { return false; }
        fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
        handle = static_cast<intptr_t>(s);
        if (allowBroadcast)
        {
            const int on = 1;
            setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof(on));
        }
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port = htons(port);
        if (::bind(s, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) { Close(); return false; }
        SocketLength length = sizeof(local);
        if (::getsockname(s, reinterpret_cast<sockaddr*>(&local), &length) == 0) { localPort = ntohs(local.sin_port); }
        return true;
    }

    void UdpSocket::Close()
    {
        if (handle != INVALID) { CloseHandle(handle); handle = INVALID; }
        localPort = 0;
    }

    bool UdpSocket::SendTo(const Address& to, const std::vector<uint8_t>& bytes)
    {
        if (handle == INVALID || bytes.empty()) { return false; }
        const sockaddr_in target = ToSockaddr(to);
        const auto sent = ::sendto(static_cast<RawSocket>(handle), reinterpret_cast<const char*>(bytes.data()),
                                   static_cast<int>(bytes.size()), 0, reinterpret_cast<const sockaddr*>(&target),
                                   sizeof(target));
        return static_cast<long long>(sent) == static_cast<long long>(bytes.size());
    }

    std::optional<std::vector<uint8_t>> UdpSocket::Receive(Address& from)
    {
        if (handle == INVALID) { return std::nullopt; }
        uint8_t buffer[1500];
        sockaddr_in source{};
        SocketLength length = sizeof(source);
        const auto got = ::recvfrom(static_cast<RawSocket>(handle), reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                    reinterpret_cast<sockaddr*>(&source), &length);
        if (got <= 0) { return std::nullopt; }
        from.ip = ntohl(source.sin_addr.s_addr);
        from.port = ntohs(source.sin_port);
        return std::vector<uint8_t>(buffer, buffer + got);
    }
}
