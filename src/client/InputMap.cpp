#include "client/InputMap.h"
#include "core/PadCursor.h"

namespace b21ui::client {
    ImGuiKey KeyFromVk(std::uint32_t vk) {
        if (vk >= 'A' && vk <= 'Z') return static_cast<ImGuiKey>(ImGuiKey_A + (vk - 'A'));
        if (vk >= '0' && vk <= '9') return static_cast<ImGuiKey>(ImGuiKey_0 + (vk - '0'));
        if (vk >= 0x70 && vk <= 0x7B) return static_cast<ImGuiKey>(ImGuiKey_F1 + (vk - 0x70));
        if (vk >= 0x60 && vk <= 0x69) return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (vk - 0x60));
        switch (vk) {
        case 0x08: return ImGuiKey_Backspace;   case 0x09: return ImGuiKey_Tab;
        case 0x0D: return ImGuiKey_Enter;       case 0x1B: return ImGuiKey_Escape;
        case 0x20: return ImGuiKey_Space;       case 0x21: return ImGuiKey_PageUp;
        case 0x22: return ImGuiKey_PageDown;    case 0x23: return ImGuiKey_End;
        case 0x24: return ImGuiKey_Home;        case 0x25: return ImGuiKey_LeftArrow;
        case 0x26: return ImGuiKey_UpArrow;     case 0x27: return ImGuiKey_RightArrow;
        case 0x28: return ImGuiKey_DownArrow;   case 0x2D: return ImGuiKey_Insert;
        case 0x2E: return ImGuiKey_Delete;
        case 0x10: case 0xA0: return ImGuiKey_LeftShift;   case 0xA1: return ImGuiKey_RightShift;
        case 0x11: case 0xA2: return ImGuiKey_LeftCtrl;    case 0xA3: return ImGuiKey_RightCtrl;
        case 0x12: case 0xA4: return ImGuiKey_LeftAlt;     case 0xA5: return ImGuiKey_RightAlt;
        case 0xBA: return ImGuiKey_Semicolon;   case 0xBB: return ImGuiKey_Equal;
        case 0xBC: return ImGuiKey_Comma;       case 0xBD: return ImGuiKey_Minus;
        case 0xBE: return ImGuiKey_Period;      case 0xBF: return ImGuiKey_Slash;
        case 0xC0: return ImGuiKey_GraveAccent; case 0xDB: return ImGuiKey_LeftBracket;
        case 0xDC: return ImGuiKey_Backslash;   case 0xDD: return ImGuiKey_RightBracket;
        case 0xDE: return ImGuiKey_Apostrophe;
        case 0x6B: return ImGuiKey_KeypadAdd;   case 0x6D: return ImGuiKey_KeypadSubtract;
        default: return ImGuiKey_None;
        }
    }

    ImGuiKey KeyFromPad(std::uint32_t padButton) {
        switch (padButton) {
        case B21UI_PAD_A: return ImGuiKey_GamepadFaceDown;
        case B21UI_PAD_B: return ImGuiKey_GamepadFaceRight;
        case B21UI_PAD_X: return ImGuiKey_GamepadFaceLeft;
        case B21UI_PAD_Y: return ImGuiKey_GamepadFaceUp;
        case B21UI_PAD_LB: return ImGuiKey_GamepadL1;
        case B21UI_PAD_RB: return ImGuiKey_GamepadR1;
        case B21UI_PAD_LT: return ImGuiKey_GamepadL2;
        case B21UI_PAD_RT: return ImGuiKey_GamepadR2;
        case B21UI_PAD_BACK: return ImGuiKey_GamepadBack;
        case B21UI_PAD_START: return ImGuiKey_GamepadStart;
        case B21UI_PAD_LS: return ImGuiKey_GamepadL3;
        case B21UI_PAD_RS: return ImGuiKey_GamepadR3;
        case B21UI_PAD_DPAD_UP: return ImGuiKey_GamepadDpadUp;
        case B21UI_PAD_DPAD_DOWN: return ImGuiKey_GamepadDpadDown;
        case B21UI_PAD_DPAD_LEFT: return ImGuiKey_GamepadDpadLeft;
        case B21UI_PAD_DPAD_RIGHT: return ImGuiKey_GamepadDpadRight;
        default: return ImGuiKey_None;
        }
    }

    namespace {
        void Stick(ImGuiIO& io, float x, float y, float deadzone, ImGuiKey left, ImGuiKey right, ImGuiKey up, ImGuiKey down) {
            x = core::Deadzone(x, deadzone);
            y = core::Deadzone(y, deadzone);
            io.AddKeyAnalogEvent(left, x < 0.0F, x < 0.0F ? -x : 0.0F);
            io.AddKeyAnalogEvent(right, x > 0.0F, x > 0.0F ? x : 0.0F);
            io.AddKeyAnalogEvent(up, y > 0.0F, y > 0.0F ? y : 0.0F);
            io.AddKeyAnalogEvent(down, y < 0.0F, y < 0.0F ? -y : 0.0F);
        }
    }

    void ApplyEvents(ImGuiIO& io, const B21UI_Frame& frame, StickState& sticks, float deadzone) {
        io.AddMousePosEvent(frame.cursorX, frame.cursorY);
        for (std::uint32_t i = 0; i < frame.eventCount; ++i) {
            const auto& e = frame.events[i];
            switch (e.type) {
            case B21UI_EV_MOUSE_BUTTON:
                if (e.code < ImGuiMouseButton_COUNT) io.AddMouseButtonEvent(static_cast<int>(e.code), e.down != 0);
                break;
            case B21UI_EV_MOUSE_WHEEL: io.AddMouseWheelEvent(0.0F, e.y); break;
            case B21UI_EV_KEY:
                if (const auto key = KeyFromVk(e.code); key != ImGuiKey_None) io.AddKeyEvent(key, e.down != 0);
                break;
            case B21UI_EV_CHAR: io.AddInputCharacterUTF16(static_cast<ImWchar16>(e.code)); break;
            case B21UI_EV_PAD_BUTTON:
                if (const auto key = KeyFromPad(e.code); key != ImGuiKey_None)
                    io.AddKeyAnalogEvent(key, e.down != 0, e.down != 0 ? (e.x > 0.0F ? e.x : 1.0F) : 0.0F);
                break;
            case B21UI_EV_PAD_STICK:
                if (e.code == B21UI_STICK_LEFT) {
                    sticks.lx = e.x; sticks.ly = e.y;
                    Stick(io, e.x, e.y, deadzone, ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight,
                          ImGuiKey_GamepadLStickUp, ImGuiKey_GamepadLStickDown);
                } else {
                    sticks.rx = e.x; sticks.ry = e.y;
                    Stick(io, e.x, e.y, deadzone, ImGuiKey_GamepadRStickLeft, ImGuiKey_GamepadRStickRight,
                          ImGuiKey_GamepadRStickUp, ImGuiKey_GamepadRStickDown);
                }
                break;
            case B21UI_EV_FOCUS: io.AddFocusEvent(e.down != 0); break;
            default: break;
            }
        }
    }
}
