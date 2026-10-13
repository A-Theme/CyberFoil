#pragma once
#include <pu/Plutonium>
#include "ui/bottomHint.hpp"
#include "util/config.hpp"
#include "util/lan_discovery.hpp"
#include "util/theme.hpp"
#include <thread>

using namespace pu::ui::elm;
namespace inst::ui {
    class optionsPage : public pu::ui::Layout
    {
        public:
            optionsPage();
            ~optionsPage();
            void stopDiscoveryWorker();
            void ShowThemePicker();
            PU_SMART_CTOR(optionsPage)
            void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);
            static void askToUpdate(std::vector<std::string> updateInfo);
            Image::Ref titleImage;
            TextBlock::Ref appVersionText;
            TextBlock::Ref timeText;
            TextBlock::Ref ipText;
            TextBlock::Ref sysLabelText;
            TextBlock::Ref sysFreeText;
            TextBlock::Ref sdLabelText;
            TextBlock::Ref sdFreeText;
            Rectangle::Ref sysBarBack;
            Rectangle::Ref sysBarFill;
            Rectangle::Ref sdBarBack;
            Rectangle::Ref sdBarFill;
            Rectangle::Ref netIndicator;
            Rectangle::Ref wifiBar1;
            Rectangle::Ref wifiBar2;
            Rectangle::Ref wifiBar3;
            Rectangle::Ref batteryOutline;
            Rectangle::Ref batteryFill;
            Rectangle::Ref batteryCap;
        private:
            std::thread discoveryThread;
            std::atomic<bool> discoveryCancel{false}, discoveryDone{false};
            inst::discovery::ScanResult discoveryResult;
            bool discoveryVisible = false, discoveryFinished = false;
            Menu::Ref discoveryMenu;
            TextBlock::Ref discoveryStatus;
            TextBlock::Ref discoverySpinner;
            u64 discoverySpinnerLastTick = 0;
            int discoverySpinnerFrame = 0;
            void startDiscovery();
            void closeDiscovery();
            void updateDiscovery(u64 down);
            BottomHintTouchState bottomHintTouch;
            std::vector<BottomHintSegment> bottomHintSegments;
            TextBlock::Ref butText;
            Rectangle::Ref topRect;
            Rectangle::Ref infoRect;
            Rectangle::Ref botRect;
            Rectangle::Ref sideNavRect;
            TextBlock::Ref pageInfoText;
            pu::ui::elm::Menu::Ref menu;
            bool touchActive = false;
            bool touchMoved = false;
            int touchStartX = 0;
            int touchStartY = 0;
            int touchRegion = 0;
            int selectedSection = 0;
            bool tabsFocused = false;
            bool remoteListVisible = false;
            bool themeListVisible = false;
            std::vector<inst::theme::Choice> themeChoices;
            void openThemeList();
            void closeThemeList();
            void handleThemeListInput(u64 down, pu::ui::Touch pos);
            bool remoteFormVisible = false;
            bool remoteFormWasActive = false;
            bool remoteFormReturnToList = false;
            int lockedMenuIndex = 0;
            int remoteListIndex = 0;
            int remoteFormReturnIndex = 0;
            inst::config::RemoteProfile remoteFormProfile;
            std::string remoteFormLanProtocol = "http";
            bool remoteFormLanProtocolDropdownVisible = false;
            std::vector<int> sectionMenuIndices;
            std::vector<inst::config::RemoteProfile> remoteListProfiles;
            std::vector<TextBlock::Ref> sectionTexts;
            std::vector<Rectangle::Ref> sectionHighlights;
            Rectangle::Ref remoteFormShade;
            Rectangle::Ref remoteFormBorder;
            Rectangle::Ref remoteFormPanel;
            TextBlock::Ref remoteFormTitle;
            TextBlock::Ref remoteFormHint;
            pu::ui::elm::Menu::Ref remoteFormMenu;
            std::vector<TextBlock::Ref> remoteFormValues;
            Rectangle::Ref remoteFormLanProtocolPanel;
            Menu::Ref remoteFormLanProtocolMenu;
            void applyRemoteFormLanProtocol();
            void setSectionNavText();
            void setSettingsMenuText();
            void refreshOptions(bool resetSelection = false);
            void openRemoteList(int selectedIndex = 0);
            void closeRemoteList();
            void manageRemote(const inst::config::RemoteProfile& remote);
            void openRemoteForm(const inst::config::RemoteProfile& remote, bool wasActive, bool returnToList, int returnIndex = 0);
            void closeRemoteForm();
            void refreshRemoteForm();
            void editRemoteFormField();
            void saveRemoteForm();
            void rememberCurrentSectionMenuIndex();
            void restoreSelectedSectionMenuIndex();
            void setSelectedSectionAndRefresh(int newSection);
            int getSectionFromTouch(int x, int y) const;
            std::string getMenuOptionIcon(bool ourBool);
            std::string getMenuLanguage(int ourLangCode);
    };
}
