#pragma once
// Pure layout rules for modern windows: how many columns fit, when the sidebar folds to icons, and
// how a user-sized window's rectangle is stored and kept on screen. No ImGui.

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace b21ui::modern::layout {
    // Columns of at least `minColumn` (plus `gap` between them) that fit in `available`, 1..maxColumns.
    constexpr int ColumnCount(float available, float minColumn, float gap, int maxColumns) {
        int columns = 1;
        while (columns < maxColumns && available >= minColumn * static_cast<float>(columns + 1) + gap * static_cast<float>(columns))
            ++columns;
        return columns;
    }

    // The sidebar shows icons only below this window width (unscaled px, compare against width / scale).
    inline constexpr float kCompactSidebarBelow = 1100.0F;
    constexpr bool CompactSidebar(float windowWidth) { return windowWidth < kCompactSidebarBelow; }
    // The burger's saved choice, overridden (not overwritten) while the window is too narrow.
    constexpr bool EffectiveCompactSidebar(bool userCompact, float windowWidth) {
        return userCompact || CompactSidebar(windowWidth);
    }

    // What the title bar's search box gets: the room the buttons leave, capped, never below `minimum`.
    constexpr float SearchWidth(float barWidth, float reserved, float preferred, float minimum) {
        return std::clamp(barWidth - reserved, minimum, std::max(minimum, preferred));
    }

    struct WindowRect {
        float x = 0, y = 0, width = 0, height = 0;
    };

    // "x,y,width,height"; nullopt for anything else.
    inline std::optional<WindowRect> ParseRect(std::string_view text) {
        float values[4]{};
        const char* at = text.data();
        const char* end = text.data() + text.size();
        for (int i = 0; i < 4; ++i) {
            while (at < end && *at == ' ') ++at;
            const auto result = std::from_chars(at, end, values[i]);
            if (result.ec != std::errc{} || result.ptr == at) return std::nullopt;
            at = result.ptr;
            while (at < end && *at == ' ') ++at;
            if (i < 3) {
                if (at == end || *at != ',') return std::nullopt;
                ++at;
            }
        }
        if (at != end) return std::nullopt;
        return WindowRect{values[0], values[1], values[2], values[3]};
    }

    inline std::string FormatRect(const WindowRect& rect) {
        char text[64];
        std::snprintf(text, sizeof(text), "%.0f,%.0f,%.0f,%.0f", rect.x, rect.y, rect.width, rect.height);
        return text;
    }

    // At least `minWidth` x `minHeight`, no larger than the display, and fully on screen.
    constexpr WindowRect FitRect(WindowRect rect, float displayWidth, float displayHeight, float minWidth, float minHeight) {
        rect.width = std::clamp(rect.width, std::min(minWidth, displayWidth), displayWidth);
        rect.height = std::clamp(rect.height, std::min(minHeight, displayHeight), displayHeight);
        rect.x = std::clamp(rect.x, 0.0F, displayWidth - rect.width);
        rect.y = std::clamp(rect.y, 0.0F, displayHeight - rect.height);
        return rect;
    }
}
