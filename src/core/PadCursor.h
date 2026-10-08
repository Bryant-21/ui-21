#pragma once

namespace b21ui::core {
    struct Vec2f {
        float x{};
        float y{};
    };

    // The map's "Normal" controller cursor speed, the shared multiplier until the map's option sets one.
    inline constexpr float kDefaultPointerMultiplier = 1.5F;

    struct PadCursorTuning {
        float deadzone = 0.2F;
        float cursorSpeed = 800.0F;
        float panSpeed = 1000.0F;
        float exponent = 1.0F;
        float pointerMultiplier = kDefaultPointerMultiplier;
    };

    // Controller pointer speed in pixels/second: the B21UI.ini base times the view's multiplier, per
    // 1080 display lines so it feels the same at any resolution.
    float PointerSpeed(float baseSpeed, float multiplier, float displayHeight);

    float Deadzone(float axis, float deadzone);
    Vec2f StickVelocity(Vec2f stick, float speed, const PadCursorTuning& tuning);
    Vec2f ClampCursor(Vec2f pos, Vec2f display, float marginX = 16.0F, float marginY = 80.0F);
}
