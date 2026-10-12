#include "util/lan_discovery.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <chrono>
#ifdef __SWITCH__
#include <switch.h>
#endif

namespace inst::discovery {
    ScanResult Scan(const std::atomic<bool>& cancel, const std::vector<std::string>& targets) {
        ScanResult result;
        if (cancel.load()) { result.cancelled = true; return result; }
        struct Socket {
            int fd = -1;
            ~Socket() { if (fd >= 0) close(fd); }
        } socketOwner;
        socketOwner.fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        const int fd = socketOwner.fd;
        if (fd < 0) { result.error = "Networking unavailable: cannot create discovery socket."; return result; }
        if (fd >= FD_SETSIZE) { result.error = "Discovery socket limit reached."; return result; }
        int enable = 1;
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        const int flags = fcntl(fd, F_GETFL, 0);
        if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0 ||
            bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0 ||
            flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            result.error = "Cannot configure discovery socket.";
            return result;
        }
        using Clock = std::chrono::steady_clock;
        const auto start = Clock::now();
        auto nextSend = start;
        int retries = 0;
        bool sent = false;
        while (Clock::now() - start < std::chrono::seconds(3)) {
            if (cancel.load()) { result.cancelled = true; result.servers.clear(); return result; }
            if (retries < 3 && Clock::now() >= nextSend) {
                for (const auto& target : targets) {
                    sockaddr_in destination{};
                    destination.sin_family = AF_INET;
                    destination.sin_port = htons(DiscoveryPort);
                    if (inet_pton(AF_INET, target.c_str(), &destination.sin_addr) == 1 &&
                        sendto(fd, Request, sizeof(Request) - 1, 0,
                            reinterpret_cast<sockaddr*>(&destination), sizeof(destination)) == sizeof(Request) - 1)
                        sent = true;
                }
                ++retries;
                nextSend = start + std::chrono::milliseconds(retries * 900);
            }
            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(fd, &readable);
            timeval timeout{0, 50000};
            const int ready = select(fd + 1, &readable, nullptr, nullptr, &timeout);
            if (ready < 0) {
                if (errno == EINTR) continue;
                result.error = "Discovery socket receive failed.";
                return result;
            }
            if (ready == 0) continue;
            // One extra byte detects truncated/oversized datagrams.
            char bytes[MaxReplyBytes + 1];
            sockaddr_in source{};
            socklen_t length = sizeof(source);
            const auto size = recvfrom(fd, bytes, sizeof(bytes), 0, reinterpret_cast<sockaddr*>(&source), &length);
            if (size < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) continue;
                result.error = "Discovery socket receive failed.";
                return result;
            }
            char ip[INET_ADDRSTRLEN];
            Server server;
            if (source.sin_family == AF_INET && inet_ntop(AF_INET, &source.sin_addr, ip, sizeof(ip)) &&
                ParseReply(bytes, static_cast<std::size_t>(size), ip, server)) AddServer(result.servers, server);
        }
        if (cancel.load()) { result.cancelled = true; result.servers.clear(); }
        else if (!sent) result.error = "Cannot send LAN discovery broadcasts. Check the network connection.";
        return result;
    }
    ScanResult ScanLan(const std::atomic<bool>& cancel) {
        if (cancel.load()) { ScanResult result; result.cancelled = true; return result; }
        std::vector<std::string> targets{"255.255.255.255"};
#ifdef __SWITCH__
        if (R_FAILED(nifmInitialize(NifmServiceType_User))) {
            ScanResult result;
            result.error = "Networking unavailable: cannot read the active interface.";
            return result;
        }
        u32 address = 0, mask = 0, gateway = 0, primaryDns = 0, secondaryDns = 0;
        const Result rc = nifmGetCurrentIpConfigInfo(&address, &mask, &gateway, &primaryDns, &secondaryDns);
        nifmExit();
        if (R_FAILED(rc) || address == 0) {
            ScanResult result;
            result.error = "No active IPv4 network. Connect to Wi-Fi or Ethernet first.";
            return result;
        }
        if (mask != 0 && mask != 0xffffffff) {
            in_addr broadcast{address | ~mask};
            char ip[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &broadcast, ip, sizeof(ip)) && targets.front() != ip) targets.emplace_back(ip);
        }
#endif
        return Scan(cancel, targets);
    }
}
