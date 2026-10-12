#include "util/lan_discovery.hpp"
#include "util/json.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cctype>

namespace inst::discovery {
    namespace {
        bool IsUuid(const std::string& s) {
            if (s.size() != 36) return false;
            for (std::size_t i = 0; i < s.size(); ++i) {
                if (i == 8 || i == 13 || i == 18 || i == 23) {
                    if (s[i] != '-') return false;
                } else if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
            }
            return true;
        }
        // Bound UTF-8 by bytes without splitting a code point. Remove control
        // characters so network-provided labels cannot insert extra UI rows.
        std::string DisplayString(const std::string& s, std::size_t limit) {
            std::size_t end = std::min(s.size(), limit);
            while (end < s.size() && end > 0 && (static_cast<unsigned char>(s[end]) & 0xc0) == 0x80) --end;
            std::string result = s.substr(0, end);
            for (char& c : result) if (static_cast<unsigned char>(c) < 32 || c == 127) c = ' ';
            return result;
        }
    }
    std::string Server::url() const {
        return "http://" + address + ":" + std::to_string(port);
    }
    bool ParseReply(const char* bytes, std::size_t size, const std::string& sourceIp, Server& out) {
        if (!bytes || size == 0 || size > MaxReplyBytes) return false;
        in_addr address{};
        if (inet_pton(AF_INET, sourceIp.c_str(), &address) != 1) return false;
        const auto ip = ntohl(address.s_addr);
        if (ip == 0 || (ip >> 24) >= 224) return false;
        try {
            // Limit nesting before building the JSON tree (unknown fields remain
            // compatible, but cannot exhaust the Switch's parser stack).
            bool tooDeep = false;
            auto j = nlohmann::json::parse(bytes, bytes + size,
                [&](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
                    if (depth > 8) tooDeep = true;
                    return depth <= 8;
                }, false);
            if (tooDeep || j.is_discarded() || !j.is_object()) return false;
            for (const char* field : {"magic", "uid", "name", "version"})
                if (!j.contains(field) || !j[field].is_string()) return false;
            if (j["magic"] != "OWNFOIL" || !j.contains("public") || !j["public"].is_boolean() ||
                !j.contains("port") || !j["port"].is_number_integer()) return false;
            if (j["port"] < 1 || j["port"] > 65535) return false;
            if (j.contains("remote") && !j["remote"].is_string()) return false;
            Server server;
            server.uid = j["uid"].get<std::string>();
            if (!IsUuid(server.uid)) return false;
            std::transform(server.uid.begin(), server.uid.end(), server.uid.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            server.name = DisplayString(j["name"].get<std::string>(), 48);
            server.version = DisplayString(j["version"].get<std::string>(), 24);
            if (j.contains("remote")) {
                const auto remote = j["remote"].get<std::string>();
                // This metadata may now become an endpoint: never truncate or
                // rewrite it into a different, apparently valid hostname/URL.
                if (remote.size() <= 512) server.remote = remote;
            }
            server.port = j["port"].get<int>();
            server.isPublic = j["public"].get<bool>();
            server.address = sourceIp;
            out = std::move(server);
            return true;
        } catch (...) { return false; }
    }
    void AddServer(std::vector<Server>& servers, const Server& server) {
        const auto found = std::find_if(servers.begin(), servers.end(),
            [&](const Server& other) { return other.uid == server.uid; });
        if (found != servers.end()) *found = server;
        else if (servers.size() < MaxServers) servers.push_back(server);
    }
}
