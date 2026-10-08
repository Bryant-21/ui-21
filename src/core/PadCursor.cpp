#include "core/PadCursor.h"

#include <algorithm>
#include <cmath>

namespace b21ui::core {
    float PointerSpeed(float baseSpeed, float multiplier, float displayHeight) {
        return baseSpeed * multiplier * std::max(1.0F, displayHeight / 1080.0F);
    }

    float Deadzone(float axis, float deadzone) {
        return std::fabs(axis) > deadzone ? axis : 0.0F;
    }

    Vec2f StickVelocity(Vec2f stick, float speed, const PadCursorTuning& tuning) {
        const auto curve = [&](float v) {
            v = Deadzone(v, tuning.deadzone);
            return std::copysign(std::pow(std::fabs(v), tuning.exponent), v) * speed;
        };
        return {curve(stick.x), -curve(stick.y)};
    }

    Vec2f ClampCursor(Vec2f pos, Vec2f display, float marginX, float marginY) {
        return {std::clamp(pos.x, marginX, std::max(marginX, display.x - marginX)),
                std::clamp(pos.y, marginY, std::max(marginY, display.y - marginY))};
    }
}
