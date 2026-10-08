#include "b21ui/CssFilter.h"

#include <doctest/doctest.h>

namespace css = b21ui::css;

namespace {
    void CheckRgb(std::array<float, 3> got, std::array<float, 3> want) {
        for (std::size_t i = 0; i < 3; ++i) CHECK(got[i] == doctest::Approx(want[i]).epsilon(0.001));
    }
}

TEST_CASE("neutral css filter amounts leave colours alone") {
    const std::array<float, 3> c{0.2F, 0.5F, 0.8F};
    for (const auto& m : {css::Sepia(0), css::Grayscale(0), css::Saturate(1), css::HueRotate(0), css::Contrast(1), css::Brightness(1)})
        CheckRgb(css::Apply(m, c), c);
}

TEST_CASE("css filter functions follow the Filter Effects matrices") {
    // grayscale(1): every channel is the luminance.
    const float y = 0.2126F * 0.2F + 0.7152F * 0.5F + 0.0722F * 0.8F;
    CheckRgb(css::Apply(css::Grayscale(1), {0.2F, 0.5F, 0.8F}), {y, y, y});
    // sepia(1) of mid grey: the sepia row sums times 0.5.
    CheckRgb(css::Apply(css::Sepia(1), {0.5F, 0.5F, 0.5F}), {0.6755F, 0.6015F, 0.4685F});
    // contrast(1.18) pivots on 0.5; brightness(0.52) scales.
    CheckRgb(css::Apply(css::Contrast(1.18F), {0.25F, 0.5F, 0.75F}), {0.205F, 0.5F, 0.795F});
    CheckRgb(css::Apply(css::Brightness(0.52F), {1, 0.5F, 0}), {0.52F, 0.26F, 0});
    // saturate(0) is the 0.213/0.715/0.072 luma.
    const float l = 0.213F * 0.2F + 0.715F * 0.5F + 0.072F * 0.8F;
    CheckRgb(css::Apply(css::Saturate(0), {0.2F, 0.5F, 0.8F}), {l, l, l});
    // hue-rotate keeps greys grey.
    CheckRgb(css::Apply(css::HueRotate(338), {0.4F, 0.4F, 0.4F}), {0.4F, 0.4F, 0.4F});
}

TEST_CASE("each css filter step clamps to the displayable range") {
    CheckRgb(css::Apply(css::Brightness(3), {0.5F, 0.1F, 0}), {1, 0.3F, 0});
    CheckRgb(css::Apply(css::Contrast(4), {0.1F, 0.5F, 0.9F}), {0, 0.5F, 1});
}
