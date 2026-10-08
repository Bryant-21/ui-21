#include "demo/Fo4Replicas.h"

#include <array>
#include <format>

namespace b21ui::demo {
    Fo4ContainerReplica::Fo4ContainerReplica()
        : player_{{"10mm Pistol", 1, true},  {"Combat Rifle"},     {"Leather Chest Piece", 1, true}, {"Stimpak", 6, false, true},
                  {"RadAway", 2},            {"Purified Water", 4}, {"Nuka-Cola"},                   {"Bobby Pin", 12},
                  {"Wrench"},                {"Baseball Bat"},      {"Pipe Wrench"}},
          container_{{"Steel", 14}, {"Adhesive"},     {"Mentats"}, {"Pipe Rifle", 1, false, false, true}, {"Duct Tape", 2}, {"Glass"},
                     {"Copper", 3}, {"Screw"},        {"Aluminum Can"}, {"Gears", 2}, {"Wood", 8}, {"Cloth", 3}} {
        playerList_.SetCount(static_cast<int>(player_.size()));
        playerList_.Select(3);
        column_ = 0;
    }

    void Fo4ContainerReplica::OnContextCreated(const FrameContext&) { fo4::InitContext(); }

    void Fo4ContainerReplica::Draw(const FrameContext&) {
        const auto nav = fo4::ReadNav();
        fo4::BeginMenu("##fo4-container");
        if (nav.dx != 0) column_ = nav.dx < 0 ? 0 : 1;
        const auto left = fo4::InventoryPanel(fo4::Side::Player, "My Inventory", player_, playerList_, column_ == 0, nav);
        const auto right = fo4::InventoryPanel(fo4::Side::Other, "Container", container_, containerList_, column_ == 1, nav);
        if (left.selectionChanged || left.activated >= 0) column_ = 0;
        if (right.selectionChanged || right.activated >= 0) column_ = 1;
        const auto move = [&](std::vector<fo4::Item>& from, fo4::ListModel& model, std::vector<fo4::Item>& to, int index) {
            if (index < 0 || index >= static_cast<int>(from.size())) return;
            to.push_back(from[static_cast<std::size_t>(index)]);
            from.erase(from.begin() + index);
            model.SetCount(static_cast<int>(from.size()));
        };
        if (right.activated >= 0) move(container_, containerList_, player_, right.activated);
        if (left.activated >= 0) move(player_, playerList_, container_, left.activated);

        const auto& items = column_ == 0 ? player_ : container_;
        const int selected = column_ == 0 ? playerList_.Selected() : containerList_.Selected();
        if (selected >= 0 && selected < static_cast<int>(items.size())) {
            const std::array<fo4::Stat, 3> stats{{{"HP", "+40"}, {"Weight", "0.1"}, {"Val", "12"}}};
            fo4::ItemCard(stats);
        }
        fo4::InventoryFooter("370/175", "110");
        const std::array<fo4::Hint, 4> hints{{{B21UI_PAD_A, "E", column_ == 0 ? "Store" : "Take"},
                                              {B21UI_PAD_X, "R", "Take All"},
                                              {B21UI_PAD_Y, "T", "Sort"},
                                              {B21UI_PAD_B, "Tab", "Exit"}}};
        if (fo4::HintBar(hints, 655.0F) == 3 || nav.cancel) b21ui::Close(*this);
        fo4::EndMenu();
    }

    Fo4PauseReplica::Fo4PauseReplica(Start start) {
        options_.SetCount(7);
        if (start == Start::Settings) {
            options_.Select(3);
            screen_ = Screen::Settings;
        } else if (start == Start::QuitPrompt) {
            options_.Select(6);
            confirmQuit_ = true;
        }
    }

    void Fo4PauseReplica::OnContextCreated(const FrameContext&) { fo4::InitContext(); }

    void Fo4PauseReplica::Draw(const FrameContext&) {
        auto nav = fo4::ReadNav();
        fo4::BeginMenu("##fo4-pause");
        static constexpr std::array<const char*, 7> options{"RESUME", "SAVE", "LOAD", "SETTINGS", "HELP", "MODS", "QUIT"};
        const auto modal = confirmQuit_ ? nav : fo4::Nav{};
        if (confirmQuit_) nav = {};

        const int activated = fo4::PauseList(options, options_, screen_ == Screen::Pause && !confirmQuit_, nav);
        if (activated == 3) screen_ = Screen::Settings;
        if (activated == 6) {
            confirmQuit_ = true;
            messageSelection_ = 0;
        }
        if (screen_ == Screen::Settings) {
            fo4::SettingsFrame();
            settings_.SetVisibleRows(fo4::kSettingRows);
            settings_.SetCount(5);
            if (nav.dy != 0) settings_.Move(nav.dy, true);
            for (int row = 0; row < 5; ++row)
                if (fo4::SettingRowHovered(row)) settings_.Select(row);
            const int s = settings_.Selected();
            static constexpr std::array<const char*, 5> difficulties{"Very Easy", "Easy", "Normal", "Hard", "Survival"};
            fo4::SettingSlider(0, "HUD Opacity", brightness_, s == 0, nav);
            fo4::SettingToggle(1, "Crosshair", crosshair_, s == 1, nav);
            fo4::SettingToggle(2, "Dialogue Subtitles", subtitles_, s == 2, nav);
            fo4::SettingStepper(3, "Difficulty", difficulty_, difficulties, s == 3, nav);
            fo4::SettingSlider(4, "Master Volume", volume_, s == 4, nav);
            if (nav.cancel) screen_ = Screen::Pause;
        } else if (nav.cancel) {
            b21ui::Close(*this);
        }

        const std::array<fo4::Hint, 2> hints{{{B21UI_PAD_A, "Enter", "Select"}, {B21UI_PAD_B, "Tab", "Back"}}};
        const int hint = fo4::HintBar(hints, 650.0F);
        if (hint == 1 && !confirmQuit_) screen_ == Screen::Settings ? void(screen_ = Screen::Pause) : b21ui::Close(*this);

        if (confirmQuit_) {
            static constexpr std::array<const char*, 2> buttons{"Yes", "No"};
            const int chosen = fo4::MessageBox("Are you sure you want to quit to the main menu?", buttons, messageSelection_, 1, modal);
            if (chosen == 0) b21ui::Close(*this);
            if (chosen >= 0) confirmQuit_ = false;
        }
        fo4::EndMenu();
    }
}
