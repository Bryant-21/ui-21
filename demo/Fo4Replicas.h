#pragma once
#include "b21ui/B21UI.h"
#include "b21ui/fo4/Style.h"

#include <vector>

namespace b21ui::demo {
    // Preview-only replicas of FO4's container / barter menu, built from the fo4 style, for side by
    // side comparison with the game's own menus.
    class Fo4ContainerReplica : public Client {
    public:
        Fo4ContainerReplica();
        void OnContextCreated(const FrameContext& frame) override;
        void Draw(const FrameContext& frame) override;

    private:
        std::vector<fo4::Item> player_, container_;
        fo4::ListModel playerList_, containerList_;
        int column_{1};
        std::string status_;
    };

    // Pause menu -> settings page (slider, toggle, stepper) -> quit confirmation message box.
    class Fo4PauseReplica : public Client {
    public:
        enum class Start { Pause, Settings, QuitPrompt };
        explicit Fo4PauseReplica(Start start = Start::Pause);
        void OnContextCreated(const FrameContext& frame) override;
        void Draw(const FrameContext& frame) override;

    private:
        enum class Screen { Pause, Settings };
        Screen screen_{Screen::Pause};
        fo4::ListModel options_, settings_;
        bool confirmQuit_{false};
        int messageSelection_{0};
        float brightness_{0.5F}, volume_{0.8F};
        bool subtitles_{true}, crosshair_{true};
        int difficulty_{2};
    };
}
