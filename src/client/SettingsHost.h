#pragma once
#include "b21ui/B21UI.h"
#include "b21ui/modern/Widgets.h"

#include <map>
#include <string>

namespace b21ui::client {
    class SettingsHost {
    public:
        // "<label> <version>", or empty when the mod sets no version.
        static std::string Title(const char* label, const char* version);
        explicit SettingsHost(std::string title = {}) : title_(std::move(title)) {}
        void Draw(Client& client, const FrameContext& frame);
        void Reset() { closeArmed_ = false; popupWasOpen_ = false; navigationRestored_ = false; }

    private:
        std::string title_;
        bool closeArmed_{};
        bool popupWasOpen_{};
        bool fullscreen_{};
        bool autoExpand_{};
        bool navigationExpanded_{};
        bool navigationRestored_{};
        std::map<std::string, bool> favorites_;
        modern::w::QuickSettingsState appearance_;
    };
}
