#pragma once
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace inst::discovery {
    inline constexpr char Request[] = "OWNFOIL_DISCOVER";
    inline constexpr int DiscoveryPort = 8465;
    inline constexpr std::size_t MaxReplyBytes = 4096;
    inline constexpr std::size_t MaxServers = 64;
    struct Server {
        std::string uid, name, version, remote, address;
        int port = 0;
        bool isPublic = false;
        std::string url() const;
    };
    bool ParseReply(const char* bytes, std::size_t size, const std::string& sourceIp, Server& out);
    void AddServer(std::vector<Server>& servers, const Server& server);
    struct ScanResult {
        std::vector<Server> servers;
        std::string error;
        bool cancelled = false;
    };
    // Called on a worker. Only this worker owns the socket; cancellation is polled
    // at least every 50 ms, including when replies arrive continuously.
    ScanResult Scan(const std::atomic<bool>& cancel, const std::vector<std::string>& targets);
    ScanResult ScanLan(const std::atomic<bool>& cancel);
}
