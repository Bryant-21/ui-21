#pragma once
#include "b21ui/B21UI.h"

#include <deque>
#include <string>

namespace b21ui::demo {
    // Kit showcase + event log. Used by the preview and the in-game demo plugin.
    class DemoClient : public Client {
    public:
        explicit DemoClient(std::string title) : title_(std::move(title)) {}
        void Draw(const FrameContext& frame) override;
        void Log(std::string line);

    private:
        std::string title_;
        std::deque<std::string> log_;
        char text_[128]{};
        float slider_{0.5F};
        int selected_{};
    };
}
