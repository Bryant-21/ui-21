#include "demo/DemoClient.h"
#include "demo/Fo4Replicas.h"
#include "preview/PreviewHost.h"

#include <string_view>

int main(int argc, char** argv) {
    std::string_view demoName = "demo";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string_view(argv[i]) == "--demo") demoName = argv[i + 1];

    static b21ui::demo::DemoClient demo("B21UI Demo");
    static b21ui::demo::Fo4ContainerReplica container;
    static b21ui::demo::Fo4PauseReplica pause;
    static b21ui::demo::Fo4PauseReplica settings(b21ui::demo::Fo4PauseReplica::Start::Settings);
    static b21ui::demo::Fo4PauseReplica quit(b21ui::demo::Fo4PauseReplica::Start::QuitPrompt);
    if (demoName == "fo4-container") b21ui::Register(container, {"fo4-container", true, false});
    else if (demoName == "fo4-pause") b21ui::Register(pause, {"fo4-pause", true, false});
    else if (demoName == "fo4-settings") b21ui::Register(settings, {"fo4-settings", true, false});
    else if (demoName == "fo4-quit") b21ui::Register(quit, {"fo4-quit", true, false});
    else b21ui::Register(demo, {"demo", true, false});
    return b21ui::preview::Run(argc, argv, std::filesystem::path("assets"));
}
