#include <doctest/doctest.h>
#include "core/PadKeyboard.h"

using namespace b21ui::core;

namespace {
    B21UI_Event Pad(std::uint32_t code, bool down) { return {B21UI_EV_PAD_BUTTON, code, down ? 1u : 0u, down ? 1.0F : 0.0F, 0.0F}; }

    std::vector<B21UI_Event> Tap(PadKeyboard& k, std::uint32_t code, bool visible = true) {
        std::vector<B21UI_Event> events{Pad(code, true), Pad(code, false)};
        k.Process(visible, events);
        return events;
    }
}

TEST_CASE("hidden keyboard passes controller input through untouched") {
    PadKeyboard k;
    const auto out = Tap(k, B21UI_PAD_A, false);
    REQUIRE(out.size() == 2);
    CHECK(out[0].type == B21UI_EV_PAD_BUTTON);
}

TEST_CASE("A types the focused key; pad buttons never reach ImGui while visible") {
    PadKeyboard k;  // starts on the q row
    auto out = Tap(k, B21UI_PAD_A);
    REQUIRE(out.size() == 1);
    CHECK(out[0].type == B21UI_EV_CHAR);
    CHECK(out[0].code == static_cast<std::uint32_t>('q'));

    Tap(k, B21UI_PAD_DPAD_RIGHT);
    Tap(k, B21UI_PAD_LT);
    out = Tap(k, B21UI_PAD_A);
    REQUIRE(out.size() == 1);
    CHECK(out[0].code == static_cast<std::uint32_t>('W'));
}

TEST_CASE("B and X map to Enter and Backspace taps") {
    PadKeyboard k;
    auto out = Tap(k, B21UI_PAD_B);
    REQUIRE(out.size() == 2);
    CHECK(out[0].type == B21UI_EV_KEY);
    CHECK(out[0].code == 0x0Du);
    CHECK(out[0].down == 1u);
    CHECK(out[1].down == 0u);
    out = Tap(k, B21UI_PAD_X);
    REQUIRE(out.size() == 2);
    CHECK(out[0].code == 0x08u);
}

TEST_CASE("a button held before the keyboard appeared releases into ImGui") {
    PadKeyboard k;
    std::vector<B21UI_Event> events{Pad(B21UI_PAD_A, true)};
    k.Process(false, events);
    REQUIRE(events.size() == 1);
    events = {Pad(B21UI_PAD_A, false)};
    k.Process(true, events);
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == B21UI_EV_PAD_BUTTON);
    CHECK(events[0].down == 0u);
}

TEST_CASE("a button pressed on the keyboard keeps its release once the keyboard hides") {
    PadKeyboard k;
    std::vector<B21UI_Event> events{Pad(B21UI_PAD_B, true)};
    k.Process(true, events);
    events = {Pad(B21UI_PAD_B, false)};
    k.Process(false, events);
    CHECK(events.empty());
}

TEST_CASE("moving down into the wide keys and back returns to the same column") {
    PadKeyboard k;
    for (int i = 0; i < 9; ++i) Tap(k, B21UI_PAD_DPAD_RIGHT);  // p
    CHECK(k.Focused().normal == 'p');
    Tap(k, B21UI_PAD_DPAD_DOWN);
    Tap(k, B21UI_PAD_DPAD_DOWN);
    Tap(k, B21UI_PAD_DPAD_DOWN);
    CHECK(k.Focused().action == PadKeyAction::Done);
    Tap(k, B21UI_PAD_DPAD_LEFT);
    CHECK(k.Focused().action == PadKeyAction::Backspace);
    Tap(k, B21UI_PAD_DPAD_DOWN);  // wraps to the number row
    CHECK(k.Focused().normal == '7');
}

TEST_CASE("left stick moves once per push and is zeroed for ImGui") {
    PadKeyboard k;
    std::vector<B21UI_Event> events{{B21UI_EV_PAD_STICK, B21UI_STICK_LEFT, 0u, 0.9F, 0.0F},
                                    {B21UI_EV_PAD_STICK, B21UI_STICK_LEFT, 0u, 1.0F, 0.0F}};
    k.Process(true, events);
    CHECK(k.Focused().normal == 'w');
    REQUIRE(events.size() == 2);
    CHECK(events[0].x == 0.0F);
}
