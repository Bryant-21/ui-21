#pragma once
#include "b21ui/B21UI.h"
#include "b21ui/modern/Widgets.h"

#include <string>

namespace b21ui::client {
    class SettingsHost {
    public:
        // "<label> <version>", or empty when the mod sets no version.
        static std::string Title(const char* label, const char* version);
        explicit SettingsHost(std::string title = {}) : title_(std::move(title)) {}
        void Draw(Client& client, const FrameContext& frame);
        void Reset() { closeArmed_ = false; popupWasOpen_ = false; }

    private:
        std::string title_;
        bool closeArmed_{};
        bool popupWasOpen_{};
        bool fullscreen_{};
        bool navigationExpanded_{true};
        modern::w::QuickSettingsState appearance_;
    };
}
