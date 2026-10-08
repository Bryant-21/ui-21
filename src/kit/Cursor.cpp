#include "b21ui/Common.h"

#include <array>

namespace b21ui::common {
    void DrawGameCursor(ImDrawList* list, ImVec2 tip, ImU32 color, float scale) {
        // Notched arrowhead in 1080p pixels: straight left edge, point at the top-left.
        constexpr std::array<ImVec2, 4> kShape{ImVec2{0.0F, 0.0F}, ImVec2{0.0F, 21.0F}, ImVec2{5.5F, 15.5F}, ImVec2{15.0F, 15.5F}};
        std::array<ImVec2, 4> p{};
        for (std::size_t i = 0; i < p.size(); ++i) p[i] = {tip.x + kShape[i].x * scale, tip.y + kShape[i].y * scale};
        const std::array<ImVec2, 4> shadow{ImVec2{p[0].x + scale, p[0].y + scale}, ImVec2{p[1].x + scale, p[1].y + scale},
                                           ImVec2{p[2].x + scale, p[2].y + scale}, ImVec2{p[3].x + scale, p[3].y + scale}};
        // The notch makes the shape concave, so it fills as two triangles.
        list->AddTriangleFilled(shadow[0], shadow[1], shadow[2], IM_COL32(0, 0, 0, 110));
        list->AddTriangleFilled(shadow[0], shadow[2], shadow[3], IM_COL32(0, 0, 0, 110));
        list->AddTriangleFilled(p[0], p[1], p[2], color);
        list->AddTriangleFilled(p[0], p[2], p[3], color);
        list->AddPolyline(p.data(), static_cast<int>(p.size()), IM_COL32(0, 0, 0, 200), ImDrawFlags_Closed, scale);
    }
}
