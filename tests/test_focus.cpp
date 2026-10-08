#include <doctest/doctest.h>
#include "core/Focus.h"

using b21ui::core::ClientKind;
using b21ui::core::FocusArbiter;

TEST_CASE("a modal client takes focus and game state") {
    FocusArbiter f;
    const auto map = f.Add(ClientKind::Modal, true);
    CHECK(map >= 1u);
    CHECK(f.Open(map));
    CHECK(f.Focused() == map);
    CHECK(f.WantsGameState());
    CHECK(f.WantsPause());
}

TEST_CASE("a second modal is refused while one is focused") {
    FocusArbiter f;
    const auto a = f.Add(ClientKind::Modal, false);
    const auto b = f.Add(ClientKind::Modal, false);
    CHECK(f.Open(a));
    CHECK_FALSE(f.Open(b));
    CHECK(f.Focused() == a);
    f.Close(a);
    CHECK(f.Focused() == 0u);
    CHECK(f.Open(b));
}

TEST_CASE("overlays open beside a modal and never take focus") {
    FocusArbiter f;
    const auto hud = f.Add(ClientKind::Overlay, false);
    const auto menu = f.Add(ClientKind::Modal, false);
    CHECK(f.Open(hud));
    CHECK(f.Focused() == 0u);
    CHECK_FALSE(f.WantsGameState());
    CHECK(f.Open(menu));
    CHECK(f.RenderOrder() == std::vector<std::uint32_t>{hud, menu});
}

TEST_CASE("pause follows only the focused modal") {
    FocusArbiter f;
    const auto quiet = f.Add(ClientKind::Modal, false);
    CHECK(f.Open(quiet));
    CHECK_FALSE(f.WantsPause());
}

TEST_CASE("unknown ids are rejected and CloseAll clears everything") {
    FocusArbiter f;
    CHECK_FALSE(f.Open(42));
    const auto a = f.Add(ClientKind::Modal, false);
    const auto o = f.Add(ClientKind::Overlay, false);
    f.Open(a);
    f.Open(o);
    f.CloseAll();
    CHECK_FALSE(f.IsOpen(a));
    CHECK_FALSE(f.IsOpen(o));
    CHECK(f.RenderOrder().empty());
}

TEST_CASE("opening an already-open modal is idempotent") {
    FocusArbiter f;
    const auto a = f.Add(ClientKind::Modal, false);
    CHECK(f.Open(a));
    CHECK(f.Open(a));
    CHECK(f.Focused() == a);
}

TEST_CASE("changing pause keeps the modal open and focused") {
    FocusArbiter focus;
    const auto map = focus.Add(ClientKind::Modal, true);
    const auto other = focus.Add(ClientKind::Modal, true);
    REQUIRE(focus.Open(map));
    REQUIRE(focus.SetPausesGame(map, false));
    CHECK_FALSE(focus.WantsPause());
    CHECK(focus.WantsGameState());
    CHECK(focus.IsOpen(map));
    CHECK(focus.Focused() == map);
    CHECK_FALSE(focus.Open(other));
    REQUIRE(focus.SetPausesGame(map, true));
    CHECK(focus.WantsPause());
    focus.Close(map);
    CHECK_FALSE(focus.WantsPause());
    CHECK_FALSE(focus.WantsGameState());
    REQUIRE(focus.Open(map));
    CHECK(focus.WantsPause());
}

TEST_CASE("pause changes to background clients leave the focused modal alone") {
    FocusArbiter focus;
    const auto map = focus.Add(ClientKind::Modal, false);
    const auto other = focus.Add(ClientKind::Modal, false);
    const auto overlay = focus.Add(ClientKind::Overlay, false);
    REQUIRE(focus.Open(map));
    REQUIRE(focus.Open(overlay));
    REQUIRE(focus.SetPausesGame(other, true));
    REQUIRE(focus.SetPausesGame(overlay, true));
    CHECK_FALSE(focus.WantsPause());
    CHECK_FALSE(focus.SetPausesGame(0, true));
    CHECK_FALSE(focus.SetPausesGame(99, true));
    focus.Close(map);
    REQUIRE(focus.Open(other));
    CHECK(focus.WantsPause());
}
