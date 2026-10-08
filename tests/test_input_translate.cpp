#include <doctest/doctest.h>
#include "core/InputTranslate.h"

using namespace b21ui::core;

namespace {
    RawInput Button(RawDevice d, std::uint32_t code, float value, float held = 0.0F) {
        return {RawKind::Button, d, code, value, held, 0, 0, 0};
    }
}

TEST_CASE("key press emits down once, repeats are ignored, release emits up") {
    InputTranslator t;
    std::vector<B21UI_Event> out;
    t.Feed(Button(RawDevice::Keyboard, 0x41, 1.0F), out);
    t.Feed(Button(RawDevice::Keyboard, 0x41, 1.0F, 0.4F), out);
    t.Feed(Button(RawDevice::Keyboard, 0x41, 0.0F, 0.5F), out);
    REQUIRE(out.size() == 2);
    CHECK(out[0].type == B21UI_EV_KEY);
    CHECK(out[0].code == 0x41u);
    CHECK(out[0].down == 1u);
    CHECK(out[1].down == 0u);
}

TEST_CASE("mouse buttons and wheel notches") {
    InputTranslator t;
    std::vector<B21UI_Event> out;
    t.Feed(Button(RawDevice::Mouse, 0, 1.0F), out);
    t.Feed(Button(RawDevice::Mouse, 0x800, 1.0F), out);
    t.Feed(Button(RawDevice::Mouse, 0x900, 1.0F), out);
    t.Feed(Button(RawDevice::Mouse, 0x900, 0.0F), out);
    t.Feed(Button(RawDevice::Mouse, 0x900, -1.0F), out);
    REQUIRE(out.size() == 4);
    CHECK(out[0].type == B21UI_EV_MOUSE_BUTTON);
    CHECK(out[1].type == B21UI_EV_MOUSE_WHEEL);
    CHECK(out[1].y == 1.0F);
    CHECK(out[2].y == -1.0F);
    CHECK(out[3].y == -1.0F);
}

TEST_CASE("gamepad buttons map to B21UI pad codes and switch the active device") {
    InputTranslator t;
    std::vector<B21UI_Event> out;
    CHECK(t.ActiveDevice() == B21UI_DEVICE_KEYBOARD_MOUSE);
    t.Feed(Button(RawDevice::Gamepad, 0x1000, 1.0F), out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].type == B21UI_EV_PAD_BUTTON);
    CHECK(out[0].code == static_cast<std::uint32_t>(B21UI_PAD_A));
    CHECK(t.ActiveDevice() == B21UI_DEVICE_GAMEPAD);
    t.Feed(Button(RawDevice::Keyboard, 0x20, 1.0F), out);
    CHECK(t.ActiveDevice() == B21UI_DEVICE_KEYBOARD_MOUSE);
}

TEST_CASE("pad code table") {
    CHECK(PadButtonFromGameCode(0x2000) == static_cast<std::uint32_t>(B21UI_PAD_B));
    CHECK(PadButtonFromGameCode(0x4000) == static_cast<std::uint32_t>(B21UI_PAD_X));
    CHECK(PadButtonFromGameCode(0x8000) == static_cast<std::uint32_t>(B21UI_PAD_Y));
    CHECK(PadButtonFromGameCode(0x0100) == static_cast<std::uint32_t>(B21UI_PAD_LB));
    CHECK(PadButtonFromGameCode(0x0200) == static_cast<std::uint32_t>(B21UI_PAD_RB));
    CHECK(PadButtonFromGameCode(0x0009) == static_cast<std::uint32_t>(B21UI_PAD_LT));
    CHECK(PadButtonFromGameCode(0x000A) == static_cast<std::uint32_t>(B21UI_PAD_RT));
    CHECK(PadButtonFromGameCode(0x0010) == static_cast<std::uint32_t>(B21UI_PAD_START));
    CHECK(PadButtonFromGameCode(0x0020) == static_cast<std::uint32_t>(B21UI_PAD_BACK));
    CHECK(PadButtonFromGameCode(0x0040) == static_cast<std::uint32_t>(B21UI_PAD_LS));
    CHECK(PadButtonFromGameCode(0x0080) == static_cast<std::uint32_t>(B21UI_PAD_RS));
    CHECK(PadButtonFromGameCode(0x0001) == static_cast<std::uint32_t>(B21UI_PAD_DPAD_UP));
    CHECK(PadButtonFromGameCode(0x0002) == static_cast<std::uint32_t>(B21UI_PAD_DPAD_DOWN));
    CHECK(PadButtonFromGameCode(0x0004) == static_cast<std::uint32_t>(B21UI_PAD_DPAD_LEFT));
    CHECK(PadButtonFromGameCode(0x0008) == static_cast<std::uint32_t>(B21UI_PAD_DPAD_RIGHT));
    CHECK_FALSE(PadButtonFromGameCode(0x7777).has_value());
}

TEST_CASE("analog triggers emit edges at half travel and carry the value") {
    InputTranslator t;
    std::vector<B21UI_Event> out;
    t.Feed(Button(RawDevice::Gamepad, 0x0009, 0.3F), out);
    t.Feed(Button(RawDevice::Gamepad, 0x0009, 0.8F, 0.1F), out);
    t.Feed(Button(RawDevice::Gamepad, 0x0009, 0.9F, 0.2F), out);
    t.Feed(Button(RawDevice::Gamepad, 0x0009, 0.1F, 0.3F), out);
    REQUIRE(out.size() == 2);
    CHECK(out[0].down == 1u);
    CHECK(out[0].x == doctest::Approx(0.8F));
    CHECK(out[1].down == 0u);
}

TEST_CASE("thumbsticks pass through and only large deflection switches device") {
    InputTranslator t;
    std::vector<B21UI_Event> out;
    t.Feed({RawKind::Thumbstick, RawDevice::Gamepad, 0xB, 0, 0, 0.1F, 0.05F, 0}, out);
    CHECK(t.ActiveDevice() == B21UI_DEVICE_KEYBOARD_MOUSE);
    t.Feed({RawKind::Thumbstick, RawDevice::Gamepad, 0xC, 0, 0, 0.0F, -0.9F, 0}, out);
    REQUIRE(out.size() == 2);
    CHECK(out[1].type == B21UI_EV_PAD_STICK);
    CHECK(out[1].code == static_cast<std::uint32_t>(B21UI_STICK_RIGHT));
    CHECK(out[1].y == doctest::Approx(-0.9F));
    CHECK(t.ActiveDevice() == B21UI_DEVICE_GAMEPAD);
}

TEST_CASE("printable characters pass through; NUL, Esc and DEL are dropped") {
    InputTranslator t;
    std::vector<B21UI_Event> out;
    t.Feed({RawKind::Character, RawDevice::Keyboard, 0, 0, 0, 0, 0, 0x00E9}, out);
    t.Feed({RawKind::Character, RawDevice::Keyboard, 0, 0, 0, 0, 0, 0}, out);
    t.Feed({RawKind::Character, RawDevice::Keyboard, 0, 0, 0, 0, 0, 0x1B}, out);
    t.Feed({RawKind::Character, RawDevice::Keyboard, 0, 0, 0, 0, 0, 0x7F}, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].type == B21UI_EV_CHAR);
    CHECK(out[0].code == 0x00E9u);
}

TEST_CASE("mouse movement switches back to keyboard and mouse") {
    InputTranslator t;
    std::vector<B21UI_Event> out;
    t.Feed(Button(RawDevice::Gamepad, 0x1000, 1.0F), out);
    t.Feed({RawKind::MouseMove, RawDevice::Mouse, 0, 0, 0, 1.0F, 0.5F, 0}, out);
    CHECK(t.ActiveDevice() == B21UI_DEVICE_GAMEPAD);
    t.Feed({RawKind::MouseMove, RawDevice::Mouse, 0, 0, 0, 6.0F, 2.0F, 0}, out);
    CHECK(t.ActiveDevice() == B21UI_DEVICE_KEYBOARD_MOUSE);
}

TEST_CASE("ReleaseAll lifts every held key, button and pad button") {
    InputTranslator t;
    std::vector<B21UI_Event> out;
    t.Feed(Button(RawDevice::Keyboard, 0x57, 1.0F), out);
    t.Feed(Button(RawDevice::Mouse, 1, 1.0F), out);
    t.Feed(Button(RawDevice::Gamepad, 0x1000, 1.0F), out);
    out.clear();
    t.ReleaseAll(out);
    REQUIRE(out.size() == 3);
    for (const auto& e : out) CHECK(e.down == 0u);
    out.clear();
    t.ReleaseAll(out);
    CHECK(out.empty());
}

TEST_CASE("a button first seen already held (the hotkey that opened the UI) is ignored until released") {
    InputTranslator t;
    std::vector<B21UI_Event> out;
    t.Feed(Button(RawDevice::Keyboard, 0x79, 1.0F, 0.2F), out);
    t.Feed(Button(RawDevice::Keyboard, 0x79, 1.0F, 0.3F), out);
    t.Feed(Button(RawDevice::Keyboard, 0x79, 0.0F, 0.4F), out);
    t.Feed(Button(RawDevice::Mouse, 0, 1.0F, 0.2F), out);
    t.Feed(Button(RawDevice::Mouse, 0, 0.0F, 0.3F), out);
    CHECK(out.empty());
    t.Feed(Button(RawDevice::Keyboard, 0x79, 1.0F), out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].type == B21UI_EV_KEY);
}
