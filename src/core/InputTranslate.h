#pragma once
#include "b21ui/Abi.h"

#include <cstdint>
#include <optional>
#include <set>
#include <vector>

namespace b21ui::core {
    enum class RawDevice : std::uint8_t { Keyboard, Mouse, Gamepad, Other };
    enum class RawKind : std::uint8_t { Button, Thumbstick, Character, MouseMove };

    // Engine-neutral copy of one game InputEvent; game/InputCapture.cpp fills it.
    struct RawInput {
        RawKind kind{};
        RawDevice device{};
        std::uint32_t idCode{};
        float value{};
        float heldSeconds{};
        float x{};
        float y{};
        std::uint32_t character{};
    };

    std::optional<std::uint32_t> PadButtonFromGameCode(std::uint32_t idCode);

    class InputTranslator {
    public:
        void Feed(const RawInput& in, std::vector<B21UI_Event>& out);
        void ReleaseAll(std::vector<B21UI_Event>& out);
        [[nodiscard]] std::uint32_t ActiveDevice() const { return device_; }

    private:
        void Edge(std::set<std::uint32_t>& held, std::uint32_t type, std::uint32_t code, bool down, float analog,
                  std::vector<B21UI_Event>& out);

        std::uint32_t device_{B21UI_DEVICE_KEYBOARD_MOUSE};
        std::set<std::uint32_t> keys_, mouse_, pad_;
    };
}
