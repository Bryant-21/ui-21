#pragma once
#include "b21ui/Abi.h"
#include "core/PadCursor.h"

#include <vector>

namespace b21ui::client {
    // Right-stick pointer for menus driven by controller focus navigation: moving the right stick shows
    // and moves a pointer, A clicks at it, and D-pad / left-stick navigation or keyboard/mouse input
    // returns to focus navigation.
    class PadPointer {
    public:
        struct Result {
            bool active{};
            bool moved{};
            float x{};
            float y{};
        };

        explicit PadPointer(bool enabled = true) : enabled_(enabled) {}
        // Copies the frame's events into `out`, turning A into a left click while pointing.
        Result Process(const B21UI_Frame& frame, const core::PadCursorTuning& tuning, std::vector<B21UI_Event>& out);
        void Reset();

    private:
        bool enabled_;
        bool active_{};
        bool aIsMouse_{};
        core::Vec2f pos_{};
        core::Vec2f stick_{};
    };
}
