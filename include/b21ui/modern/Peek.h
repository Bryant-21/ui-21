#pragma once
// Quick peek: the title-bar eye that makes a modern window almost invisible so the scene behind it
// shows. A click toggles it; pressing and holding peeks only while held. The window keeps its input,
// so the eye stays clickable while peeking. Pure: no ImGui.

#include <cmath>
#include <cstdint>

namespace b21ui::modern {
    // Alpha of everything the window draws (background, text, widgets) while peeking.
    inline constexpr float kPeekAlpha = 0.10F;
    // A press held at least this long is a hold, not a click.
    inline constexpr float kPeekHoldSeconds = 0.30F;

    class Peek {
    public:
        void Press() { held_ = true; }
        void Release(float heldSeconds) {
            held_ = false;
            if (heldSeconds < kPeekHoldSeconds) toggled_ = !toggled_;
        }
        void Set(bool on) {
            toggled_ = on;
            held_ = false;
        }
        bool Toggled() const { return toggled_; }
        bool Active() const { return toggled_ || held_; }
        float Opacity() const { return Active() ? kPeekAlpha : 1.0F; }

    private:
        bool toggled_ = false;
        bool held_ = false;
    };

    // `color` (ImU32, alpha in the top byte) with its alpha scaled by `factor`, never above the original.
    constexpr std::uint32_t FadeColor(std::uint32_t color, float factor) {
        const float clamped = factor < 0.0F ? 0.0F : factor > 1.0F ? 1.0F : factor;
        const auto alpha = static_cast<std::uint32_t>(static_cast<float>(color >> 24) * clamped + 0.5F);
        return (color & 0x00FFFFFFu) | (alpha << 24);
    }
}
