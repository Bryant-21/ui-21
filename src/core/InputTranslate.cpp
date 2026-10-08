#include "core/InputTranslate.h"

#include <cmath>

namespace b21ui::core {
    namespace {
        constexpr std::uint32_t kWheelUp = 0x800, kWheelDown = 0x900;
        constexpr std::uint32_t kLeftTrigger = 0x9, kRightTrigger = 0xA;
        constexpr std::uint32_t kLeftStick = 0xB;
        constexpr float kStickSwitch = 0.2F, kTriggerDown = 0.5F, kMouseSwitch = 4.0F;

        // The game also sends Esc, Backspace, Enter and Tab as CharacterEvents (M0 Q7); those
        // already arrive as key events, so only printable text passes.
        bool Printable(std::uint32_t ch) { return ch >= 0x20 && ch != 0x7F; }

        // A button first seen already held (heldSeconds > 0; FO4's own QJustPressed needs 0) was pressed
        // before the UI started listening, e.g. the hotkey that opened it, whose repeats arrive once the
        // deferred Open lands. Reporting it as a fresh press closed UIs bound to that key on open.
        bool StaleHold(const std::set<std::uint32_t>& held, std::uint32_t code, bool down, float heldSeconds) {
            return down && heldSeconds > 0.0F && !held.contains(code);
        }
    }

    std::optional<std::uint32_t> PadButtonFromGameCode(std::uint32_t idCode) {
        switch (idCode) {
        case 0x1000: return B21UI_PAD_A;
        case 0x2000: return B21UI_PAD_B;
        case 0x4000: return B21UI_PAD_X;
        case 0x8000: return B21UI_PAD_Y;
        case 0x0100: return B21UI_PAD_LB;
        case 0x0200: return B21UI_PAD_RB;
        case kLeftTrigger: return B21UI_PAD_LT;
        case kRightTrigger: return B21UI_PAD_RT;
        case 0x0020: return B21UI_PAD_BACK;
        case 0x0010: return B21UI_PAD_START;
        case 0x0040: return B21UI_PAD_LS;
        case 0x0080: return B21UI_PAD_RS;
        case 0x0001: return B21UI_PAD_DPAD_UP;
        case 0x0002: return B21UI_PAD_DPAD_DOWN;
        case 0x0004: return B21UI_PAD_DPAD_LEFT;
        case 0x0008: return B21UI_PAD_DPAD_RIGHT;
        default: return std::nullopt;
        }
    }

    void InputTranslator::Edge(std::set<std::uint32_t>& held, std::uint32_t type, std::uint32_t code, bool down,
                               float analog, std::vector<B21UI_Event>& out) {
        const bool wasDown = held.contains(code);
        if (down == wasDown) return;
        if (down) held.insert(code);
        else held.erase(code);
        out.push_back({type, code, down ? 1u : 0u, analog, 0.0F});
    }

    void InputTranslator::Feed(const RawInput& in, std::vector<B21UI_Event>& out) {
        switch (in.kind) {
        case RawKind::Character:
            if (Printable(in.character)) out.push_back({B21UI_EV_CHAR, in.character, 1u, 0.0F, 0.0F});
            return;
        case RawKind::MouseMove:
            if (std::fabs(in.x) + std::fabs(in.y) > kMouseSwitch) device_ = B21UI_DEVICE_KEYBOARD_MOUSE;
            return;
        case RawKind::Thumbstick: {
            if (std::fabs(in.x) > kStickSwitch || std::fabs(in.y) > kStickSwitch) device_ = B21UI_DEVICE_GAMEPAD;
            const std::uint32_t stick = in.idCode == kLeftStick ? B21UI_STICK_LEFT : B21UI_STICK_RIGHT;
            out.push_back({B21UI_EV_PAD_STICK, stick, 1u, in.x, in.y});
            return;
        }
        case RawKind::Button:
            break;
        }
        const bool pressed = in.value > 0.0F;
        switch (in.device) {
        case RawDevice::Keyboard:
            if (pressed) device_ = B21UI_DEVICE_KEYBOARD_MOUSE;
            if (StaleHold(keys_, in.idCode, pressed, in.heldSeconds)) break;
            Edge(keys_, B21UI_EV_KEY, in.idCode, pressed, in.value, out);
            break;
        case RawDevice::Mouse:
            if (pressed) device_ = B21UI_DEVICE_KEYBOARD_MOUSE;
            if (in.idCode == kWheelUp || in.idCode == kWheelDown) {
                // The direction comes from the code; FO4 may sign the value by direction, so any non-zero press counts.
                if (in.value != 0.0F && in.heldSeconds == 0.0F)
                    out.push_back({B21UI_EV_MOUSE_WHEEL, 0u, 1u, 0.0F, in.idCode == kWheelUp ? 1.0F : -1.0F});
            } else if (in.idCode < 8) {
                if (StaleHold(mouse_, in.idCode, pressed, in.heldSeconds)) break;
                Edge(mouse_, B21UI_EV_MOUSE_BUTTON, in.idCode, pressed, in.value, out);
            }
            break;
        case RawDevice::Gamepad: {
            const auto pad = PadButtonFromGameCode(in.idCode);
            if (!pad) break;
            const bool trigger = in.idCode == kLeftTrigger || in.idCode == kRightTrigger;
            const bool down = trigger ? in.value > kTriggerDown : pressed;
            if (down) device_ = B21UI_DEVICE_GAMEPAD;
            if (!trigger && StaleHold(pad_, *pad, down, in.heldSeconds)) break;
            Edge(pad_, B21UI_EV_PAD_BUTTON, *pad, down, in.value, out);
            break;
        }
        case RawDevice::Other:
            break;
        }
    }

    void InputTranslator::ReleaseAll(std::vector<B21UI_Event>& out) {
        for (auto code : keys_) out.push_back({B21UI_EV_KEY, code, 0u, 0.0F, 0.0F});
        for (auto code : mouse_) out.push_back({B21UI_EV_MOUSE_BUTTON, code, 0u, 0.0F, 0.0F});
        for (auto code : pad_) out.push_back({B21UI_EV_PAD_BUTTON, code, 0u, 0.0F, 0.0F});
        keys_.clear();
        mouse_.clear();
        pad_.clear();
    }
}
