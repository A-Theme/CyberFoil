// Host POSIX tests: compile the production parser and scanner, without Switch/UI.
#include "util/lan_discovery.hpp"
#include "util/json.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <dirent.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace inst::discovery;
using namespace std::chrono_literals;
static void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static nlohmann::json Reply() {
    return {{"magic", "OWNFOIL"}, {"uid", "12345678-1234-4abc-8def-123456789abc"},
        {"name", "AeroFoil"}, {"version", "1.2.3"}, {"port", 8080}, {"public", false}};
}
static bool Parse(const nlohmann::json& j, Server& server) {
    auto text = j.dump();
    return ParseReply(text.data(), text.size(), "192.168.1.12", server);
}
static void ParserTests() {
    Server server;
    auto j = Reply();
    Check(Parse(j, server), "valid reply without optional remote");
    Check(server.url() == "http://192.168.1.12:8080", "URL uses advertised HTTP port");
    j["remote"] = "https://outside.example:443/path";
    j["future"] = {{"anything", 123}};
    Check(Parse(j, server) && server.url() == "http://192.168.1.12:8080", "remote and unknown fields do not affect LAN URL");
    for (int port : {1, 65535}) {
        j["port"] = port;
        Check(Parse(j, server) && server.port == port, "valid port boundary");
    }
    for (const auto& port : std::vector<nlohmann::json>{0, -1, 65536, "8080", 8080.0, true, nullptr, UINT64_MAX}) {
        j["port"] = port;
        Check(!Parse(j, server), "invalid port or type");
    }
    for (const char* field : {"magic", "uid", "name", "version", "port", "public"}) {
        j = Reply(); j.erase(field);
        Check(!Parse(j, server), "missing required field");
        j = Reply(); j[field] = nlohmann::json::array();
        Check(!Parse(j, server), "wrong field type");
    }
    j = Reply(); j["magic"] = "AEROFOIL"; Check(!Parse(j, server), "wrong protocol magic");
    j = Reply(); j["uid"] = "not-a-uuid"; Check(!Parse(j, server), "invalid UUID");
    j = Reply(); j["remote"] = nullptr; Check(!Parse(j, server), "wrong optional field type");
    for (const std::string& text : {std::string("{"), std::string("[]"), std::string("null"),
            std::string(MaxReplyBytes + 1, 'x'), std::string("{\"name\":\"\xff\"}")})
        Check(!ParseReply(text.data(), text.size(), "192.168.1.12", server), "malformed or oversized reply");
    auto text = Reply().dump();
    for (const char* ip : {"not-an-ip", "0.0.0.0", "255.255.255.255", "224.0.0.1", "::1"})
        Check(!ParseReply(text.data(), text.size(), ip, server), "invalid source IP");
    j = Reply();
    nlohmann::json deep = "leaf";
    for (int i = 0; i < 16; ++i) deep = nlohmann::json::array({deep});
    j["future"] = deep; Check(!Parse(j, server), "bounded nesting");
    j = Reply(); j["name"] = std::string(47, 'a') + "é\n";
    j["version"] = std::string(100, 'v');
    j["remote"] = std::string(300, 'r');
    Check(Parse(j, server) && server.name.size() == 47 && server.version.size() == 24 && server.remote.size() == 300,
        "UTF-8-safe displayed string bounds");
    j["remote"] = std::string(513, 'r');
    Check(Parse(j, server) && server.remote.empty(), "oversized remote metadata ignored without truncating a URL");
    j["name"] = "Hi\nthere\u0000!";
    Check(Parse(j, server) && server.name.find('\n') == std::string::npos, "display controls sanitized");
    std::vector<Server> servers;
    AddServer(servers, server); AddServer(servers, server);
    Check(servers.size() == 1, "duplicate uid");
    j["uid"] = "12345678-1234-4ABC-8DEF-123456789ABC";
    Check(Parse(j, server), "uppercase UUID");
    AddServer(servers, server); Check(servers.size() == 1, "UUID case deduplication");
    server.uid = "different"; AddServer(servers, server); Check(servers.size() == 2, "multiple servers");
    for (int i = 0; i < 100; ++i) { server.uid = std::to_string(i); AddServer(servers, server); }
    Check(servers.size() == MaxServers, "bounded results");
}

static int CountFds() {
    DIR* directory = opendir("/proc/self/fd");
    if (!directory) return -1; // Optional Linux leak check; other POSIX hosts can run the tests.
    int count = 0;
    while (readdir(directory)) ++count;
    closedir(directory);
    return count;
}
static void ScanTests() {
    std::atomic<bool> cancel{true};
    Check(Scan(cancel, {"127.0.0.1"}).cancelled, "pre-cancelled scan");
    struct Socket { int fd = socket(AF_INET, SOCK_DGRAM, 0); ~Socket() { close(fd); } } responder;
    Check(responder.fd >= 0, "responder socket");
    sockaddr_in local{}; local.sin_family = AF_INET; local.sin_port = htons(DiscoveryPort);
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Check(bind(responder.fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == 0, "bind test responder");
    const int initialFds = CountFds();
    cancel.store(false);
    ScanResult result;
    std::thread scan([&] { result = Scan(cancel, {"127.0.0.1"}); });
    int requests = 0;
    std::vector<std::chrono::steady_clock::time_point> retryTimes;
    const auto deadline = std::chrono::steady_clock::now() + 3200ms;
    bool requestValid = true;
    while (std::chrono::steady_clock::now() < deadline) {
        fd_set set; FD_ZERO(&set); FD_SET(responder.fd, &set);
        timeval wait{0, 100000};
        if (select(responder.fd + 1, &set, nullptr, nullptr, &wait) <= 0) continue;
        char bytes[64]; sockaddr_in peer{}; socklen_t length = sizeof(peer);
        int size = recvfrom(responder.fd, bytes, sizeof(bytes), 0, reinterpret_cast<sockaddr*>(&peer), &length);
        requestValid &= size == static_cast<int>(sizeof(Request) - 1) && std::string(bytes, size) == Request;
        ++requests;
        retryTimes.push_back(std::chrono::steady_clock::now());
        auto j = Reply();
        for (int i = 0; i < 3; ++i) {
            if (i == 2) j["uid"] = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee";
            auto reply = j.dump();
            sendto(responder.fd, reply.data(), reply.size(), 0, reinterpret_cast<sockaddr*>(&peer), length);
        }
        const std::string huge(MaxReplyBytes + 100, 'x');
        sendto(responder.fd, huge.data(), huge.size(), 0, reinterpret_cast<sockaddr*>(&peer), length);
        sendto(responder.fd, "bad", 3, 0, reinterpret_cast<sockaddr*>(&peer), length);
    }
    scan.join();
    Check(requestValid && requests == 3, "exact discovery bytes and three retries");
    Check(retryTimes[1] - retryTimes[0] >= 700ms && retryTimes[2] - retryTimes[1] >= 700ms, "spaced retries");
    Check(result.error.empty() && !result.cancelled && result.servers.size() == 2, "unicast collection and deduplication");
    Check(result.servers.front().url() == "http://127.0.0.1:8080", "HTTP port differs from UDP source port");
    for (int i = 0; i < 3; ++i) {
        cancel.store(false);
        std::thread worker([&] { result = Scan(cancel, {"127.0.0.1"}); });
        std::this_thread::sleep_for(80ms);
        const auto start = std::chrono::steady_clock::now();
        cancel.store(true); worker.join();
        Check(result.cancelled && result.servers.empty(), "active/repeated cancellation");
        Check(std::chrono::steady_clock::now() - start < 250ms, "prompt cancellation");
    }
    cancel.store(false);
    result = Scan(cancel, {"invalid-target"});
    Check(!result.error.empty() && result.servers.empty(), "send failure");
    result = Scan(cancel, {"127.0.0.1"}); // responder is idle: no valid replies
    Check(result.error.empty() && result.servers.empty() && !result.cancelled, "no results and scan after cancellation");
    Check(CountFds() == initialFds, "socket cleanup on success, send failure, and repeated cancellation");
}
int main() {
    try { ParserTests(); ScanTests(); std::cout << "LAN discovery tests passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
