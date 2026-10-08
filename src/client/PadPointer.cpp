#include "client/PadPointer.h"

#include <algorithm>
#include <cmath>

namespace b21ui::client {
    namespace {
        bool IsDpad(std::uint32_t pad) {
            return pad == B21UI_PAD_DPAD_UP || pad == B21UI_PAD_DPAD_DOWN || pad == B21UI_PAD_DPAD_LEFT ||
                   pad == B21UI_PAD_DPAD_RIGHT;
        }
    }

    void PadPointer::Reset() {
        active_ = false;
        aIsMouse_ = false;
        stick_ = {};
    }

    PadPointer::Result PadPointer::Process(const B21UI_Frame& frame, const core::PadCursorTuning& tuning,
                                           std::vector<B21UI_Event>& out) {
        out.assign(frame.events, frame.events + frame.eventCount);
        if (!enabled_) return {false, false, frame.cursorX, frame.cursorY};
        if (frame.activeDevice == B21UI_DEVICE_KEYBOARD_MOUSE) active_ = false;
        for (auto& e : out) {
            if (e.type == B21UI_EV_PAD_STICK && e.code == B21UI_STICK_RIGHT) {
                stick_ = {e.x, e.y};
            } else if (e.type == B21UI_EV_PAD_STICK) {
                if (std::fabs(e.x) > tuning.deadzone || std::fabs(e.y) > tuning.deadzone) active_ = false;
            } else if (e.type == B21UI_EV_PAD_BUTTON && IsDpad(e.code) && e.down) {
                active_ = false;
            } else if (e.type == B21UI_EV_PAD_BUTTON && e.code == B21UI_PAD_A) {
                // The release follows its press even if the mode ended in between, so no button sticks.
                const bool asMouse = e.down ? active_ : aIsMouse_;
                aIsMouse_ = e.down && active_;
                if (asMouse) e = {B21UI_EV_MOUSE_BUTTON, 0u, e.down, 0.0F, 0.0F};
            }
        }
        const auto speed = core::PointerSpeed(tuning.cursorSpeed, tuning.pointerMultiplier, frame.displayHeight);
        const auto velocity = core::StickVelocity(stick_, speed, tuning);
        const bool moving = velocity.x != 0.0F || velocity.y != 0.0F;
        if (moving && !active_) {
            active_ = true;
            pos_ = {frame.cursorX, frame.cursorY};
        }
        if (!active_) return {false, false, frame.cursorX, frame.cursorY};
        if (moving) {
            pos_.x = std::clamp(pos_.x + velocity.x * frame.deltaSeconds, 0.0F, std::max(0.0F, frame.displayWidth - 1.0F));
            pos_.y = std::clamp(pos_.y + velocity.y * frame.deltaSeconds, 0.0F, std::max(0.0F, frame.displayHeight - 1.0F));
        }
        return {true, moving, pos_.x, pos_.y};
    }
}
