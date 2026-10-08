#include <doctest/doctest.h>
#include "core/PadCursor.h"

using namespace b21ui::core;

TEST_CASE("deadzone zeroes small deflection and passes the rest unchanged") {
    CHECK(Deadzone(0.2F, 0.2F) == 0.0F);
    CHECK(Deadzone(-0.15F, 0.2F) == 0.0F);
    CHECK(Deadzone(0.5F, 0.2F) == doctest::Approx(0.5F));
    CHECK(Deadzone(-1.0F, 0.2F) == doctest::Approx(-1.0F));
}

TEST_CASE("stick velocity is linear by default and flips y to screen space") {
    const PadCursorTuning t;
    const auto v = StickVelocity({1.0F, 1.0F}, t.cursorSpeed, t);
    CHECK(v.x == doctest::Approx(800.0F));
    CHECK(v.y == doctest::Approx(-800.0F));
    const auto half = StickVelocity({0.5F, 0.0F}, t.cursorSpeed, t);
    CHECK(half.x == doctest::Approx(400.0F));
    const auto dead = StickVelocity({0.1F, -0.1F}, t.cursorSpeed, t);
    CHECK(dead.x == 0.0F);
    CHECK(dead.y == 0.0F);
}

TEST_CASE("an exponent above one slows fine movement but keeps full speed") {
    PadCursorTuning t;
    t.exponent = 2.0F;
    CHECK(StickVelocity({0.5F, 0.0F}, 800.0F, t).x == doctest::Approx(200.0F));
    CHECK(StickVelocity({-1.0F, 0.0F}, 800.0F, t).x == doctest::Approx(-800.0F));
}

TEST_CASE("cursor clamps to the map margins") {
    const auto c = ClampCursor({-5.0F, 2000.0F}, {1920.0F, 1080.0F});
    CHECK(c.x == 16.0F);
    CHECK(c.y == 1000.0F);
}

TEST_CASE("pointer speed matches the map: base x multiplier, scaled per 1080 lines, never below 1x") {
    using namespace b21ui::core;
    CHECK(PointerSpeed(800.0F, kDefaultPointerMultiplier, 1080.0F) == doctest::Approx(1200.0F));
    CHECK(PointerSpeed(800.0F, kDefaultPointerMultiplier, 2160.0F) == doctest::Approx(2400.0F));
    CHECK(PointerSpeed(800.0F, kDefaultPointerMultiplier, 720.0F) == doctest::Approx(1200.0F));
}
