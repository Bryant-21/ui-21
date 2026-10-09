#pragma once
#include "b21ui/B21UI.h"
#include "b21ui/modern/Widgets.h"

namespace b21ui::client {
    class SettingsHost {
    public:
        void Draw(Client& client, const FrameContext& frame);
        void Reset() { closeArmed_ = false; popupWasOpen_ = false; }

    private:
        bool closeArmed_{};
        bool popupWasOpen_{};
        bool fullscreen_{};
        bool navigationExpanded_{true};
        modern::w::QuickSettingsState appearance_;
    };
}
