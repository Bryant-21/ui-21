#include "demo/DemoClient.h"
#include "demo/Fo4Replicas.h"
#include "preview/PreviewHost.h"
#include "preview/McmDemo.h"
#include "preview/KeybindingsDemo.h"
#include "b21ui/Settings.h"
#include "b21ui/modern/Widgets.h"

#include <string_view>

namespace {
    class SettingsDemo final : public b21ui::Client {
    public:
        explicit SettingsDemo(const char* label) : label_(label) {}
        void Draw(const b21ui::FrameContext&) override {
            ImGui::SetCursorPos({24, 24});
            b21ui::modern::w::Title(label_);
            if (b21ui::modern::w::Switch("Enabled", enabled_)) enabled_ = !enabled_;
        }
    private:
        const char* label_;
        bool enabled_{};
    };
}

int main(int argc, char** argv) {
    std::string_view demoName = "demo";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string_view(argv[i]) == "--demo") demoName = argv[i + 1];

    static b21ui::demo::DemoClient demo("B21UI Demo");
    static b21ui::demo::Fo4ContainerReplica container;
    static b21ui::demo::Fo4PauseReplica pause;
    static b21ui::demo::Fo4PauseReplica settings(b21ui::demo::Fo4PauseReplica::Start::Settings);
    static b21ui::demo::Fo4PauseReplica quit(b21ui::demo::Fo4PauseReplica::Start::QuitPrompt);
    static SettingsDemo first("Example Mod"), second("Another Mod");
    if (demoName == "keybindings") b21ui::preview::RegisterKeybindingsDemo();
    else if (demoName == "settings" || demoName == "mcm") {
        B21UI_ClientId mcmPanel{};
        if (demoName == "mcm") {
            b21ui::preview::RegisterMcmDemo(argc, argv);
            const auto panels = b21ui::SettingsPanels();
            if (!panels.empty()) mcmPanel = panels.back().id;
        }
        b21ui::Register(first, {.name = "exampleSettings", .settings = true, .settingsLabel = "Example Mod",
            .settingsIcon = b21ui::modern::icon::Gear});
        b21ui::Register(second, {.name = "otherSettings", .settings = true, .settingsLabel = "Another Mod",
            .settingsIcon = b21ui::modern::icon::Terminal});
        b21ui::OpenSettings(mcmPanel);
    }
    else if (demoName == "fo4-container") b21ui::Register(container, {"fo4-container", true, false});
    else if (demoName == "fo4-pause") b21ui::Register(pause, {"fo4-pause", true, false});
    else if (demoName == "fo4-settings") b21ui::Register(settings, {"fo4-settings", true, false});
    else if (demoName == "fo4-quit") b21ui::Register(quit, {"fo4-quit", true, false});
    else b21ui::Register(demo, {"demo", true, false});
    return b21ui::preview::Run(argc, argv, std::filesystem::path("assets"));
}
