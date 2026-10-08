#pragma once

#include <array>
#include <cmath>
#include <numbers>

// CSS filter functions (Filter Effects Module 1, §13) as colour matrices, so a PrismaUI page's
// `filter:` chain can be reproduced on an image: run the steps in order, clamping to [0, 1] after each.
namespace b21ui::css {
    // out = row . (r, g, b, 1) for each of r, g, b.
    struct ColorMatrix {
        std::array<std::array<float, 4>, 3> rows{{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}};
    };

    inline ColorMatrix Grayscale(float amount) {
        const float a = 1 - std::fmin(1.0F, std::fmax(0.0F, amount));
        return {{{{0.2126F + 0.7874F * a, 0.7152F - 0.7152F * a, 0.0722F - 0.0722F * a, 0},
                  {0.2126F - 0.2126F * a, 0.7152F + 0.2848F * a, 0.0722F - 0.0722F * a, 0},
                  {0.2126F - 0.2126F * a, 0.7152F - 0.7152F * a, 0.0722F + 0.9278F * a, 0}}}};
    }

    inline ColorMatrix Sepia(float amount) {
        const float a = 1 - std::fmin(1.0F, std::fmax(0.0F, amount));
        return {{{{0.393F + 0.607F * a, 0.769F - 0.769F * a, 0.189F - 0.189F * a, 0},
                  {0.349F - 0.349F * a, 0.686F + 0.314F * a, 0.168F - 0.168F * a, 0},
                  {0.272F - 0.272F * a, 0.534F - 0.534F * a, 0.131F + 0.869F * a, 0}}}};
    }

    inline ColorMatrix Saturate(float s) {
        return {{{{0.213F + 0.787F * s, 0.715F - 0.715F * s, 0.072F - 0.072F * s, 0},
                  {0.213F - 0.213F * s, 0.715F + 0.285F * s, 0.072F - 0.072F * s, 0},
                  {0.213F - 0.213F * s, 0.715F - 0.715F * s, 0.072F + 0.928F * s, 0}}}};
    }

    inline ColorMatrix HueRotate(float degrees) {
        const float r = degrees * std::numbers::pi_v<float> / 180.0F, c = std::cos(r), s = std::sin(r);
        return {{{{0.213F + c * 0.787F - s * 0.213F, 0.715F - c * 0.715F - s * 0.715F, 0.072F - c * 0.072F + s * 0.928F, 0},
                  {0.213F - c * 0.213F + s * 0.143F, 0.715F + c * 0.285F + s * 0.140F, 0.072F - c * 0.072F - s * 0.283F, 0},
                  {0.213F - c * 0.213F - s * 0.787F, 0.715F - c * 0.715F + s * 0.715F, 0.072F + c * 0.928F + s * 0.072F, 0}}}};
    }

    inline ColorMatrix Contrast(float amount) {
        const float offset = 0.5F - 0.5F * amount;
        return {{{{amount, 0, 0, offset}, {0, amount, 0, offset}, {0, 0, amount, offset}}}};
    }

    inline ColorMatrix Brightness(float amount) { return {{{{amount, 0, 0, 0}, {0, amount, 0, 0}, {0, 0, amount, 0}}}}; }

    // One step on an sRGB colour in [0, 1], clamped like the browser does between filter functions.
    inline std::array<float, 3> Apply(const ColorMatrix& m, std::array<float, 3> rgb) {
        std::array<float, 3> out{};
        for (int i = 0; i < 3; ++i) {
            const auto& row = m.rows[static_cast<std::size_t>(i)];
            const float v = row[0] * rgb[0] + row[1] * rgb[1] + row[2] * rgb[2] + row[3];
            out[static_cast<std::size_t>(i)] = std::fmin(1.0F, std::fmax(0.0F, v));
        }
        return out;
    }
}
