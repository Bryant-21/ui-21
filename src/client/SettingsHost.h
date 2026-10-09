#pragma once
#include "b21ui/B21UI.h"
#include "b21ui/modern/Widgets.h"

namespace b21ui::client {
    class SettingsHost {
    public:
        void Draw(Client& client, const FrameContext& frame);
        void Reset() { closeArmed_ = false; }

    private:
        bool closeArmed_{};
        bool fullscreen_{};
        modern::w::QuickSettingsState appearance_;
    };
}
