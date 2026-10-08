#pragma once
#include "b21ui/Abi.h"

#include <array>
#include <cstdint>
#include <set>
#include <span>
#include <vector>

namespace b21ui::core {
    enum class PadKeyAction : std::uint8_t { Char, Shift, Space, Backspace, Done };

    struct PadKey {
        char normal;
        char shifted;
        PadKeyAction action;
        std::uint8_t span;  // columns out of PadKeyboard::kColumns
        const char* label;  // non-Char keys
    };

    // On-screen keyboard for controller users. While visible it rewrites a frame's controller input
    // into the key and character events a keyboard would send to the focused text field, so every
    // ImGui text widget works without knowing about it. Pad buttons never reach ImGui meanwhile
    // (its gamepad navigation would move focus off the field), and sticks are zeroed.
    class PadKeyboard {
    public:
        static constexpr int kColumns = 10;
        static std::span<const std::span<const PadKey>> Rows();

        void Process(bool visible, std::vector<B21UI_Event>& events);

        [[nodiscard]] int Row() const { return row_; }
        [[nodiscard]] int Column() const { return column_; }
        [[nodiscard]] bool Shifted() const { return shift_; }
        [[nodiscard]] const PadKey& Focused() const;
        [[nodiscard]] static int KeyStart(std::span<const PadKey> row, std::size_t index);

    private:
        void Move(int dx, int dy);
        void Press(const PadKey& key, std::vector<B21UI_Event>& out) const;
        void Handle(std::uint32_t pad, std::vector<B21UI_Event>& out);

        int row_{1};
        int column_{0};
        bool shift_{false};
        int stickX_{0};
        int stickY_{0};
        std::set<std::uint32_t> swallowed_;
    };
}
