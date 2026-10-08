#include <doctest/doctest.h>

#include "b21ui/modern/AppearanceCodec.h"
#include "b21ui/modern/Peek.h"

#include <map>
#include <optional>
#include <string>

using b21ui::modern::Peek;
namespace appearance = b21ui::modern::appearance;
using b21ui::modern::theme::Appearance;

TEST_CASE("peek: a click toggles, a second click restores") {
    Peek peek;
    CHECK_FALSE(peek.Active());
    peek.Press();
    CHECK(peek.Active());
    peek.Release(0.1F);
    CHECK(peek.Active());
    peek.Press();
    peek.Release(0.1F);
    CHECK_FALSE(peek.Active());
}

TEST_CASE("peek: holding peeks only while held") {
    Peek peek;
    peek.Press();
    CHECK(peek.Active());
    peek.Release(b21ui::modern::kPeekHoldSeconds + 0.2F);
    CHECK_FALSE(peek.Active());

    // A hold while toggled on peeks on and leaves it on.
    peek.Press();
    peek.Release(0.05F);
    REQUIRE(peek.Active());
    peek.Press();
    peek.Release(1.0F);
    CHECK(peek.Active());
}

TEST_CASE("peek: the window fades to the peek alpha and back") {
    Peek peek;
    CHECK(peek.Opacity() == doctest::Approx(1.0F));
    peek.Set(true);
    CHECK(peek.Opacity() == doctest::Approx(b21ui::modern::kPeekAlpha));
    CHECK(b21ui::modern::kPeekAlpha == doctest::Approx(0.10F));
}

TEST_CASE("peek: fading scales alpha only and rounds") {
    using b21ui::modern::FadeColor;
    CHECK(FadeColor(0xFF112233u, 1.0F) == 0xFF112233u);
    CHECK(FadeColor(0xFF112233u, 0.10F) == 0x1A112233u);  // 255 * 0.1 = 25.5 -> 26
    CHECK(FadeColor(0x80FFFFFFu, 0.5F) == 0x40FFFFFFu);
    CHECK(FadeColor(0xFFABCDEFu, 0.0F) == 0x00ABCDEFu);
    CHECK(FadeColor(0x10ABCDEFu, 3.0F) == 0x10ABCDEFu);  // never brighter than drawn
}

namespace {
    appearance::Lookup From(const std::map<std::string, std::string>& values) {
        return [values](std::string_view key) -> std::optional<std::string> {
            const auto found = values.find(std::string(key));
            return found == values.end() ? std::nullopt : std::optional(found->second);
        };
    }
}

TEST_CASE("appearance: each client has its own section, the shared one is the old store") {
    CHECK(appearance::Section("") == "Modern");
    CHECK(appearance::Section("FO4CS") == "Modern.FO4CS");
}

TEST_CASE("appearance: the client's value wins, then the shared one, then the client's default") {
    Appearance defaults;
    defaults.backgroundOpacity = 0.6F;
    const auto empty = From({});

    auto resolved = appearance::Resolve(empty, empty, defaults);
    CHECK(resolved.backgroundOpacity == doctest::Approx(0.6F));
    CHECK(resolved.textScale == doctest::Approx(1.0F));

    resolved = appearance::Resolve(empty, From({{"BackgroundOpacity", "0.85"}}), defaults);
    CHECK(resolved.backgroundOpacity == doctest::Approx(0.85F));

    resolved = appearance::Resolve(From({{"BackgroundOpacity", "0.45"}}), From({{"BackgroundOpacity", "0.85"}}), defaults);
    CHECK(resolved.backgroundOpacity == doctest::Approx(0.45F));
}

TEST_CASE("appearance: values clamp, garbage keeps the default, and the writer round-trips") {
    Appearance defaults;
    const auto empty = From({});
    auto resolved = appearance::Resolve(From({{"BackgroundOpacity", "0.01"}, {"TextScale", "9"}, {"TableTextSize", "abc"}}),
                                        empty, defaults);
    CHECK(resolved.backgroundOpacity == doctest::Approx(b21ui::modern::theme::MinBackgroundOpacity));
    CHECK(resolved.textScale == doctest::Approx(b21ui::modern::theme::MaxTextScale));
    CHECK(resolved.tableTextSize == doctest::Approx(defaults.tableTextSize));

    Appearance custom{0.55F, 1.2F, 16.0F};
    std::map<std::string, std::string> written;
    for (const auto& [key, value] : appearance::Format(custom)) written[key] = value;
    CHECK(written.size() == 3);
    const auto back = appearance::Resolve(From(written), empty, defaults);
    CHECK(back.backgroundOpacity == doctest::Approx(0.55F));
    CHECK(back.textScale == doctest::Approx(1.2F));
    CHECK(back.tableTextSize == doctest::Approx(16.0F));
}

#include "b21ui/modern/Layout.h"

namespace layout = b21ui::modern::layout;

TEST_CASE("layout: two columns only when both reach their minimum width") {
    CHECK(layout::ColumnCount(1400.0F, 520.0F, 16.0F, 2) == 2);
    CHECK(layout::ColumnCount(1056.0F, 520.0F, 16.0F, 2) == 2);
    CHECK(layout::ColumnCount(1055.0F, 520.0F, 16.0F, 2) == 1);
    CHECK(layout::ColumnCount(300.0F, 520.0F, 16.0F, 2) == 1);
    CHECK(layout::ColumnCount(5000.0F, 520.0F, 16.0F, 2) == 2);
    CHECK(layout::ColumnCount(5000.0F, 520.0F, 16.0F, 3) == 3);
}

TEST_CASE("layout: the sidebar folds to icons below the breakpoint") {
    CHECK(layout::CompactSidebar(900.0F));
    CHECK_FALSE(layout::CompactSidebar(1400.0F));
}

TEST_CASE("layout: the search box shrinks before the buttons, down to its minimum") {
    CHECK(layout::SearchWidth(1600.0F, 600.0F, 480.0F, 140.0F) == doctest::Approx(480.0F));
    CHECK(layout::SearchWidth(900.0F, 600.0F, 480.0F, 140.0F) == doctest::Approx(300.0F));
    CHECK(layout::SearchWidth(600.0F, 600.0F, 480.0F, 140.0F) == doctest::Approx(140.0F));
}

TEST_CASE("layout: a window rect round-trips and rejects junk") {
    const auto rect = layout::ParseRect(layout::FormatRect({10, 20, 1280, 720}));
    REQUIRE(rect);
    CHECK(rect->x == doctest::Approx(10.0F));
    CHECK(rect->height == doctest::Approx(720.0F));
    CHECK_FALSE(layout::ParseRect(""));
    CHECK_FALSE(layout::ParseRect("1,2,3"));
    CHECK_FALSE(layout::ParseRect("1,2,3,4,5"));
    CHECK_FALSE(layout::ParseRect("a,b,c,d"));
}

TEST_CASE("layout: a stored rect is clamped to the minimum size and kept on screen") {
    auto fit = layout::FitRect({-50, 900, 200, 100}, 1920, 1080, 800, 500);
    CHECK(fit.width == doctest::Approx(800.0F));
    CHECK(fit.height == doctest::Approx(500.0F));
    CHECK(fit.x == doctest::Approx(0.0F));
    CHECK(fit.y == doctest::Approx(580.0F));
    // A rect saved on a bigger display shrinks to this one.
    fit = layout::FitRect({0, 0, 3000, 2000}, 1920, 1080, 800, 500);
    CHECK(fit.width == doctest::Approx(1920.0F));
    CHECK(fit.height == doctest::Approx(1080.0F));
}

TEST_CASE("layout: the burger's choice holds unless the window is too narrow, and comes back") {
    CHECK_FALSE(layout::EffectiveCompactSidebar(false, 1400.0F));
    CHECK(layout::EffectiveCompactSidebar(true, 1400.0F));
    CHECK(layout::EffectiveCompactSidebar(false, 900.0F));  // forced while narrow
    CHECK(layout::EffectiveCompactSidebar(true, 900.0F));
    CHECK_FALSE(layout::EffectiveCompactSidebar(false, 1400.0F));  // widened: the saved choice again
}

TEST_CASE("appearance: the theme family round-trips through its stored name, unknown reads as modern") {
    using b21ui::modern::theme::Family;
    for (const auto family : {Family::Modern, Family::Fallout4, Family::Fallout76})
        CHECK(appearance::ParseFamily(appearance::FamilyName(family)) == family);
    CHECK(appearance::FamilyName(Family::Fallout4) == "fo4");
    CHECK(appearance::FamilyName(Family::Fallout76) == "fo76");
    CHECK(appearance::ParseFamily("") == Family::Modern);
    CHECK(appearance::ParseFamily("skyrim") == Family::Modern);
    CHECK(appearance::ParseFamily("FO4") == Family::Modern);
}

TEST_CASE("appearance: the client's theme wins, then the shared one, then the default") {
    using b21ui::modern::theme::Family;
    Appearance defaults;
    const auto empty = From({});
    CHECK(appearance::Resolve(empty, empty, defaults).family == Family::Modern);
    CHECK(appearance::Resolve(empty, From({{"Theme", "fo76"}}), defaults).family == Family::Fallout76);
    CHECK(appearance::Resolve(From({{"Theme", "fo4"}}), From({{"Theme", "fo76"}}), defaults).family == Family::Fallout4);
    CHECK(appearance::Resolve(From({{"Theme", "nonsense"}}), empty, defaults).family == Family::Modern);
    // Sliders save without pinning the theme, so a window keeps following the shared choice.
    for (const auto& [key, value] : appearance::Format(Appearance{}))
        CHECK(key != "Theme");
}
