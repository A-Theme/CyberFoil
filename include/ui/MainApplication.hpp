#pragma once
#include <pu/Plutonium>
#include <switch.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include "util/ui_dispatcher.hpp"
#include "ui/mainPage.hpp"
#include "ui/netInstPage.hpp"
#include "ui/remoteInstPage.hpp"
#include "ui/sdInstPage.hpp"
#include "ui/usbInstPage.hpp"
#include "ui/hddInstPage.hpp"
#include "ui/instPage.hpp"
#include "ui/optionsPage.hpp"

namespace inst::ui {
    class MainApplication;
    extern MainApplication *mainApp;

    class MainApplication : public pu::ui::Application {
        public:
            using Application::Application;
            PU_SMART_CTOR(MainApplication)
            ~MainApplication();
            void Close();
            void OnLoad() override;
            void RefreshInputDevice(bool force = false);
            void RecreateLayouts();
            void RequestThemeReload();
            bool IsUiThread() const { return uiDispatcher.isOwner(); }
            bool IsInstallWorkActive() const { return installWorkActive.load(); }
            template<class F> auto RunOnUiThread(F fn) -> std::invoke_result_t<F> { return uiDispatcher.invoke(std::move(fn)); }
            void PostInstallUiUpdate(const std::string& key, std::function<void()> fn) { uiDispatcher.postLatest(key, std::move(fn)); }
            void RunInstallWork(std::function<void()> work);
            bool CallForRender();
            int CreateShowDialog(const std::string& title, const std::string& content, std::vector<std::string> options, bool useLastOptionAsCancel, const std::string& icon = "");
            pu::ui::Layout::Ref GetCurrentLayout() const { return this->lyt; }
            MainPage::Ref mainPage;
            netInstPage::Ref netinstPage;
            remoteInstPage::Ref remoteinstPage;
            sdInstPage::Ref sdinstPage;
            usbInstPage::Ref usbinstPage;
            hddInstPage::Ref hddinstPage;
            instPage::Ref instpage;
            optionsPage::Ref optionspage;
        private:
            inst::UiDispatcher uiDispatcher;
            std::atomic<bool> installWorkActive{false};
            bool themeReloadPending = false;
            int themeReloadStage = -1;
            unsigned themeLoadingFrame = 0;
            pu::ui::Layout::Ref themeLoadingLayout;
            std::vector<pu::ui::elm::Rectangle::Ref> themeLoadingDots;
            pu::ui::elm::Rectangle::Ref themeLoadingProgress;
            void CreateLayoutStep(int step);
            void ShowThemeLoading();
            void ApplyPendingTheme();
            // Fetches the theme's web images for the next launch. Started after
            // every theme load; stopped before the next one and on exit.
            std::thread themeRefreshThread;
            std::atomic<bool> themeRefreshStop{false};
            std::atomic<bool> themeRefreshDone{true};
            // "Applying theme..." holds until a theme's first web downloads land,
            // or this deadline passes, so a newly picked theme opens with them.
            bool themeAwaitDownloads = false;
            std::chrono::steady_clock::time_point themeAwaitDeadline;
            void StartThemeRefresh();
            void StopThemeRefresh();
            void ReapplyBackgrounds();
            AppletFocusState lastFocusState = AppletFocusState_InFocus;
    };
}
