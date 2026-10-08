#include "core/PadKeyboard.h"

#include <cmath>

namespace b21ui::core {
    namespace {
        constexpr std::uint32_t kVkBack = 0x08, kVkReturn = 0x0D, kVkLeft = 0x25, kVkRight = 0x27;
        constexpr float kStickEdge = 0.6F;

        constexpr PadKey C(char normal, char shifted) { return {normal, shifted, PadKeyAction::Char, 1, nullptr}; }

        constexpr std::array kNumbers{C('1', '!'), C('2', '@'), C('3', '#'), C('4', '$'), C('5', '%'),
                                      C('6', '^'), C('7', '&'), C('8', '*'), C('9', '('), C('0', ')')};
        constexpr std::array kTop{C('q', 'Q'), C('w', 'W'), C('e', 'E'), C('r', 'R'), C('t', 'T'),
                                  C('y', 'Y'), C('u', 'U'), C('i', 'I'), C('o', 'O'), C('p', 'P')};
        constexpr std::array kHome{C('a', 'A'), C('s', 'S'), C('d', 'D'), C('f', 'F'), C('g', 'G'),
                                   C('h', 'H'), C('j', 'J'), C('k', 'K'), C('l', 'L'), C('\'', '"')};
        constexpr std::array kBottom{C('z', 'Z'), C('x', 'X'), C('c', 'C'), C('v', 'V'), C('b', 'B'),
                                     C('n', 'N'), C('m', 'M'), C(',', ';'), C('.', ':'), C('-', '_')};
        constexpr std::array kSpecial{PadKey{0, 0, PadKeyAction::Shift, 2, "Shift"}, PadKey{0, 0, PadKeyAction::Space, 4, "Space"},
                                      PadKey{0, 0, PadKeyAction::Backspace, 2, "Delete"}, PadKey{0, 0, PadKeyAction::Done, 2, "Done"}};
        constexpr std::array<std::span<const PadKey>, 5> kRows{kNumbers, kTop, kHome, kBottom, kSpecial};

        std::size_t KeyAt(std::span<const PadKey> row, int column) {
            int start = 0;
            for (std::size_t i = 0; i < row.size(); ++i) {
                start += row[i].span;
                if (column < start) return i;
            }
            return row.size() - 1;
        }

        void Tap(std::uint32_t vk, std::vector<B21UI_Event>& out) {
            out.push_back({B21UI_EV_KEY, vk, 1u, 0.0F, 0.0F});
            out.push_back({B21UI_EV_KEY, vk, 0u, 0.0F, 0.0F});
        }

        int StickStep(float value) { return value > kStickEdge ? 1 : value < -kStickEdge ? -1 : 0; }
    }

    std::span<const std::span<const PadKey>> PadKeyboard::Rows() { return kRows; }

    int PadKeyboard::KeyStart(std::span<const PadKey> row, std::size_t index) {
        int start = 0;
        for (std::size_t i = 0; i < index; ++i) start += row[i].span;
        return start;
    }

    const PadKey& PadKeyboard::Focused() const {
        const auto row = kRows[static_cast<std::size_t>(row_)];
        return row[KeyAt(row, column_)];
    }

    void PadKeyboard::Move(int dx, int dy) {
        const int rows = static_cast<int>(kRows.size());
        if (dy != 0) row_ = (row_ + dy + rows) % rows;
        if (dx != 0) {
            const auto row = kRows[static_cast<std::size_t>(row_)];
            const int count = static_cast<int>(row.size());
            const int next = (static_cast<int>(KeyAt(row, column_)) + dx + count) % count;
            column_ = KeyStart(row, static_cast<std::size_t>(next));
        }
    }

    void PadKeyboard::Press(const PadKey& key, std::vector<B21UI_Event>& out) const {
        switch (key.action) {
        case PadKeyAction::Char:
            out.push_back({B21UI_EV_CHAR, static_cast<std::uint32_t>(static_cast<unsigned char>(shift_ ? key.shifted : key.normal)), 1u, 0.0F, 0.0F});
            break;
        case PadKeyAction::Space: out.push_back({B21UI_EV_CHAR, static_cast<std::uint32_t>(' '), 1u, 0.0F, 0.0F}); break;
        case PadKeyAction::Backspace: Tap(kVkBack, out); break;
        case PadKeyAction::Done: Tap(kVkReturn, out); break;
        case PadKeyAction::Shift: break;
        }
    }

    void PadKeyboard::Handle(std::uint32_t pad, std::vector<B21UI_Event>& out) {
        switch (pad) {
        case B21UI_PAD_DPAD_UP: Move(0, -1); break;
        case B21UI_PAD_DPAD_DOWN: Move(0, 1); break;
        case B21UI_PAD_DPAD_LEFT: Move(-1, 0); break;
        case B21UI_PAD_DPAD_RIGHT: Move(1, 0); break;
        case B21UI_PAD_A:
            if (Focused().action == PadKeyAction::Shift) shift_ = !shift_;
            else Press(Focused(), out);
            break;
        case B21UI_PAD_X: Tap(kVkBack, out); break;
        case B21UI_PAD_Y: out.push_back({B21UI_EV_CHAR, static_cast<std::uint32_t>(' '), 1u, 0.0F, 0.0F}); break;
        case B21UI_PAD_LT:
        case B21UI_PAD_LS: shift_ = !shift_; break;
        case B21UI_PAD_LB: Tap(kVkLeft, out); break;
        case B21UI_PAD_RB: Tap(kVkRight, out); break;
        // Enter, not Escape: Escape would revert the field and throw away what was typed.
        case B21UI_PAD_B:
        case B21UI_PAD_START: Tap(kVkReturn, out); break;
        default: break;
        }
    }

    void PadKeyboard::Process(bool visible, std::vector<B21UI_Event>& events) {
        std::vector<B21UI_Event> out;
        out.reserve(events.size());
        for (const auto& e : events) {
            if (e.type == B21UI_EV_PAD_BUTTON) {
                // A button that was down before the keyboard appeared (the A that focused the field)
                // still releases into ImGui, so ImGui never sees it stuck down.
                if (!e.down) {
                    if (swallowed_.erase(e.code) == 0) out.push_back(e);
                } else if (!visible) {
                    out.push_back(e);
                } else if (swallowed_.insert(e.code).second) {
                    Handle(e.code, out);
                }
                continue;
            }
            if (e.type == B21UI_EV_PAD_STICK && visible) {
                if (e.code == B21UI_STICK_LEFT) {
                    const int x = StickStep(e.x), y = StickStep(e.y);
                    if (x != 0 && x != stickX_) Move(x, 0);
                    if (y != 0 && y != stickY_) Move(0, -y);
                    stickX_ = x;
                    stickY_ = y;
                }
                out.push_back({e.type, e.code, 0u, 0.0F, 0.0F});
                continue;
            }
            out.push_back(e);
        }
        if (!visible) stickX_ = stickY_ = 0;
        events.swap(out);
    }
}
