// "remote_images" parsing: which web addresses a theme may name, and where
// each one's download is cached. Host-only; no SDL, network or Switch SDK.
#include "util/theme_remote.hpp"
#include <iostream>
#include <set>
#include <stdexcept>

namespace remote = inst::theme::remote;
static void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static void UrlTests() {
    Check(remote::IsWebUrl("https://a-theme.ca/images/Alienware.jpg"), "https URL");
    Check(remote::IsWebUrl("http://gandalfsax.com/images/gaming.jpg"), "http URL");
    Check(remote::IsWebUrl("https://example.com/random?seed=1&w=1280"), "query string");
    Check(!remote::IsWebUrl("background.jpg"), "a bundled file name is not a web URL");
    Check(!remote::IsWebUrl("sdmc:/switch/prod.keys"), "device path");
    Check(!remote::IsWebUrl("file:///etc/passwd"), "file URL");
    Check(!remote::IsWebUrl("ftp://example.com/a.png"), "ftp URL");
    Check(!remote::IsWebUrl("https://"), "scheme only");
    Check(!remote::IsWebUrl("https://exa mple.com/a.png"), "space");
    Check(!remote::IsWebUrl("https://example.com/\x01.png"), "control character");
    Check(!remote::IsWebUrl("https://example.com/" + std::string(3000, 'a')), "overlong URL");
}

static void CacheTests() {
    const auto a = remote::CachePath("background.jpg", "https://a.example/x.jpg");
    Check(a.rfind(".web/background.jpg-", 0) == 0 && a.size() > 4 && a.substr(a.size() - 4) == ".img", "cache path shape");
    Check(a == remote::CachePath("background.jpg", "https://a.example/x.jpg"), "same URL, same cache file");
    Check(a != remote::CachePath("background.jpg", "https://b.example/x.jpg"), "a new URL does not reuse the old site's image");
    Check(remote::CachePath("background.jpg", "https://a.example/x.jpg") ==
          ".web/background.jpg-" + a.substr(20, 8) + ".img", "hash is eight hex digits");
    // FNV-1a of the empty string is its offset basis: pins the hash, so a
    // cache written by one build is found by the next.
    Check(remote::CachePath("k", "") == ".web/k-811c9dc5.img", "stable hash");
    const auto icon = remote::CachePath("icons/remote.png", "https://a.example/i.png");
    Check(icon.find('/', 5) == std::string::npos, "a nested romfs key is flattened to one directory");
}

static void ParseTests() {
    const auto root = nlohmann::json::parse(R"({
        "images": {"background": "background.jpg"},
        "remote_images": {
            "background": "https://a-theme.ca/images/Alienware.jpg",
            "logo": "https://a-theme.ca/images/alogo.jpg",
            "icons/remote.png": "https://example.com/icon.png",
            "bad": "sdmc:/switch/prod.keys",
            "number": 4,
            "../escape": "https://example.com/x.png"
        }
    })");
    const auto images = remote::Parse(root);
    std::set<std::string> keys;
    for (const auto& image : images) keys.insert(image.key);
    Check(images.size() == 3, "three valid entries");
    Check(keys.count("background.jpg") && keys.count("logo.png"), "background and logo shorthand normalized");
    Check(keys.count("icons/remote.png"), "any image slot may be remote");
    Check(!keys.count("bad") && !keys.count("number") && !keys.count("../escape"), "invalid entries skipped");
    for (const auto& image : images) Check(image.cache == remote::CachePath(image.key, image.url), "cache filled in");

    Check(remote::Parse(nlohmann::json::parse(R"({"images":{}})")).empty(), "no remote_images");
    Check(remote::Parse(nlohmann::json::parse(R"({"remote_images":"https://a.example/x.jpg"})")).empty(), "a string is not a map");
    Check(remote::Parse(nlohmann::json::parse("[]")).empty(), "not an object");

    nlohmann::json many = {{"remote_images", nlohmann::json::object()}};
    for (int i = 0; i < 20; ++i) many["remote_images"]["k" + std::to_string(i)] = "https://example.com/" + std::to_string(i);
    Check(remote::Parse(many).size() == remote::kMaxImages, "bounded number of downloads");
}

int main() {
    try {
        UrlTests();
        CacheTests();
        ParseTests();
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    std::cout << "theme remote tests passed\n";
    return 0;
}
