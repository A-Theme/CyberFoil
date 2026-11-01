#include "util/theme_remote.hpp"

#include <cstdio>

namespace inst::theme::remote {
    std::string NormalizeKey(const std::string& key) {
        if (key == "background") return "background.jpg";
        if (key == "logo") return "logo.png";
        return key;
    }

    bool IsWebUrl(const std::string& url) {
        if (url.size() > 2048) return false;
        const bool https = url.rfind("https://", 0) == 0;
        if (!https && url.rfind("http://", 0) != 0) return false;
        if (url.size() <= (https ? 8u : 7u)) return false;
        for (const unsigned char ch : url) if (ch <= 0x20 || ch >= 0x7F) return false;
        return true;
    }

    std::string CachePath(const std::string& key, const std::string& url) {
        std::uint32_t hash = 2166136261u;
        for (const unsigned char ch : url) { hash ^= ch; hash *= 16777619u; }
        // The key names a romfs asset (e.g. "icons/remote.png"); flatten it so
        // the cache stays one directory deep.
        std::string flat;
        for (const char ch : key) flat += (ch == '/' || ch == '\\' || ch == ':') ? '_' : ch;
        char suffix[16];
        std::snprintf(suffix, sizeof(suffix), "-%08x", static_cast<unsigned>(hash));
        return ".web/" + flat + suffix + ".img";
    }

    std::vector<Image> Parse(const nlohmann::json& root) {
        std::vector<Image> out;
        if (!root.is_object() || !root.contains("remote_images") || !root["remote_images"].is_object()) return out;
        for (const auto& entry : root["remote_images"].items()) {
            if (out.size() >= kMaxImages) break;
            if (!entry.value().is_string()) continue;
            const auto url = entry.value().get<std::string>();
            const auto key = NormalizeKey(entry.key());
            if (key.empty() || key.find("..") != std::string::npos || !IsWebUrl(url)) continue;
            out.push_back({key, url, CachePath(key, url)});
        }
        return out;
    }
}
