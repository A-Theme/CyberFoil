#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "util/json.hpp"

// Theme images fetched from the web, the SDL- and network-free half.
//
// A theme may list web addresses next to its bundled images:
//
//     "images":        { "background": "background.jpg" },
//     "remote_images": { "background": "https://example.com/random.jpg" }
//
// Tinfoil themes point at servers that answer with a different picture every
// time, so the wallpaper changes from one launch to the next. CyberFoil keeps
// that rotation without ever waiting on the network: Load() uses the image the
// previous launch downloaded (falling back to the bundled one), and a worker
// fetches a fresh one for the launch after.
//
// A build without this support ignores "remote_images" and shows the bundled
// image, so a theme can carry both and work everywhere.
namespace inst::theme::remote {
    struct Image {
        std::string key;    // normalized slot, e.g. "background.jpg"
        std::string url;
        std::string cache;  // relative to the theme folder, e.g. ".web/background.jpg-1a2b3c4d.img"
    };

    constexpr std::uint64_t kMaxImageBytes = 16ULL * 1024 * 1024;  // ValidImage's own ceiling
    constexpr long kDownloadTimeoutMs = 8000;
    constexpr std::size_t kMaxImages = 8;

    // "background" and "logo" are accepted as shorthand, as in "images".
    std::string NormalizeKey(const std::string& key);
    // http(s) only, printable ASCII, no spaces, bounded length.
    bool IsWebUrl(const std::string& url);
    // Stable across builds (FNV-1a), so a cache survives an update; keyed on the
    // URL too, so editing the address does not keep showing the old site's image.
    std::string CachePath(const std::string& key, const std::string& url);
    // Every valid entry of root["remote_images"], in key order, at most kMaxImages.
    std::vector<Image> Parse(const nlohmann::json& root);
}
