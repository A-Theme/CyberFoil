#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <pu/ui/ui_Layout.hpp>
#include <pu/ui/elm/elm_Menu.hpp>

namespace inst::theme {
    // Load on the UI thread before creating or recreating themed layouts.
    struct Choice {
        std::string file; // Relative to appDir/theme; empty selects the built-in appearance.
        std::string name;
    };
    std::vector<Choice> Discover(const std::string& appDir);
    void Load(const std::string& appDir, const std::string& selectedFile = "theme.json");
    // Web images named under "remote_images" (see util/theme_remote.hpp).
    struct RemoteDownload {
        std::string url;
        std::string path;   // where the next launch looks for it
    };
    // The loaded theme's web images. Copy taken on the UI thread, then handed
    // to a worker - the worker never reads theme state.
    std::vector<RemoteDownload> RemoteImages();
    // Fetch a fresh copy of each for the NEXT launch. Blocking; run it on a
    // worker. Checks `stop` between images, and each download is time-limited.
    void RefreshRemoteImages(const std::vector<RemoteDownload>& images, const std::atomic<bool>& stop);
    std::string Name();
    bool HasColor(const std::string& role);
    pu::ui::Color Color(const std::string& role, pu::ui::Color fallback);
    pu::ui::Color Palette(const std::string& fallbackHex);
    std::string Asset(const std::string& defaultPath);
    void ApplyBackground(pu::ui::Layout& layout, bool oledMode, const std::string& appDir);

    // Apply focus text colors without modifying the bundled Plutonium library.
    class Menu : public pu::ui::elm::Menu {
    public:
        Menu(s32 x, s32 y, s32 width, pu::ui::Color optionColor, s32 itemSize, s32 itemsToShow, s32 fontSize = 25);
        PU_SMART_CTOR(Menu)
        void OnRender(pu::ui::render::Renderer::Ref& drawer, s32 x, s32 y) override;
    private:
        std::weak_ptr<pu::ui::elm::MenuItem> focusedItem;
        pu::ui::Color originalColor;
    };
}
