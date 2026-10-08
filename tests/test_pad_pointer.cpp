#include <doctest/doctest.h>
#include "client/PadPointer.h"

#include <vector>

using b21ui::client::PadPointer;

namespace {
    struct Step {
        std::vector<B21UI_Event> out;
        b21ui::client::PadPointer::Result result;
    };

    Step Run(PadPointer& p, std::vector<B21UI_Event> events, std::uint32_t device = B21UI_DEVICE_GAMEPAD,
             float dt = 0.1F) {
        B21UI_Frame f{};
        f.size = sizeof(B21UI_Frame);
        f.displayWidth = 1000;
        f.displayHeight = 800;
        f.deltaSeconds = dt;
        f.cursorX = 500;
        f.cursorY = 400;
        f.activeDevice = device;
        f.events = events.data();
        f.eventCount = static_cast<std::uint32_t>(events.size());
        Step s;
        s.result = p.Process(f, {}, s.out);
        return s;
    }

    B21UI_Event Stick(std::uint32_t which, float x, float y) { return {B21UI_EV_PAD_STICK, which, 1, x, y}; }
    B21UI_Event Pad(std::uint32_t button, bool down) { return {B21UI_EV_PAD_BUTTON, button, down ? 1u : 0u, 1, 0}; }
}

TEST_CASE("right stick starts pointer mode and moves the pointer from the game cursor") {
    PadPointer p;
    auto s = Run(p, {Stick(B21UI_STICK_RIGHT, 1.0F, 0.0F)});
    CHECK(s.result.active);
    CHECK(s.result.moved);
    CHECK(s.result.x == doctest::Approx(620.0F));  // 800 base x 1.5 (the map's Normal) x 0.1 s
    CHECK(s.result.y == doctest::Approx(400.0F));
    s = Run(p, {});   // stick still held: the game only sends stick events on change
    CHECK(s.result.x == doctest::Approx(740.0F));
}

TEST_CASE("pointer stays inside the display") {
    PadPointer p;
    const auto s = Run(p, {Stick(B21UI_STICK_RIGHT, 1.0F, 1.0F)}, B21UI_DEVICE_GAMEPAD, 10.0F);
    CHECK(s.result.x == doctest::Approx(999.0F));
    CHECK(s.result.y == doctest::Approx(0.0F));
}

TEST_CASE("A clicks the mouse while pointing, and its release follows even after leaving the mode") {
    PadPointer p;
    Run(p, {Stick(B21UI_STICK_RIGHT, 0.5F, 0.0F)});
    auto s = Run(p, {Stick(B21UI_STICK_RIGHT, 0.0F, 0.0F), Pad(B21UI_PAD_A, true)});
    REQUIRE(s.out.size() == 2);
    CHECK(s.out[1].type == B21UI_EV_MOUSE_BUTTON);
    CHECK(s.out[1].code == 0u);
    CHECK(s.out[1].down == 1u);
    s = Run(p, {Pad(B21UI_PAD_DPAD_DOWN, true), Pad(B21UI_PAD_A, false)});
    CHECK_FALSE(s.result.active);
    REQUIRE(s.out.size() == 2);
    CHECK(s.out[0].type == B21UI_EV_PAD_BUTTON);
    CHECK(s.out[1].type == B21UI_EV_MOUSE_BUTTON);
    CHECK(s.out[1].down == 0u);
}

TEST_CASE("A stays a nav press outside pointer mode") {
    PadPointer p;
    const auto s = Run(p, {Pad(B21UI_PAD_A, true)});
    REQUIRE(s.out.size() == 1);
    CHECK(s.out[0].type == B21UI_EV_PAD_BUTTON);
    CHECK_FALSE(s.result.active);
}

TEST_CASE("left stick navigation and keyboard/mouse both end pointer mode") {
    PadPointer p;
    Run(p, {Stick(B21UI_STICK_RIGHT, 1.0F, 0.0F)});
    CHECK(Run(p, {Stick(B21UI_STICK_RIGHT, 0.0F, 0.0F)}).result.active);
    CHECK_FALSE(Run(p, {Stick(B21UI_STICK_LEFT, 0.0F, 0.9F)}).result.active);
    Run(p, {Stick(B21UI_STICK_RIGHT, 1.0F, 0.0F)});
    CHECK(Run(p, {Stick(B21UI_STICK_RIGHT, 0.0F, 0.0F)}).result.active);
    CHECK_FALSE(Run(p, {}, B21UI_DEVICE_KEYBOARD_MOUSE).result.active);
}

TEST_CASE("a disabled pointer passes everything through") {
    PadPointer p(false);
    const auto s = Run(p, {Stick(B21UI_STICK_RIGHT, 1.0F, 0.0F), Pad(B21UI_PAD_A, true)});
    CHECK_FALSE(s.result.active);
    CHECK(s.out.size() == 2);
}
