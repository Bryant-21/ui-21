#include "client/PadKeyboardView.h"
#include "b21ui/Common.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

namespace b21ui::client {
    namespace {
        ImU32 Hud(int alpha = 255) { return (common::HudColor() & ~IM_COL32_A_MASK) | static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT; }

        void CenteredText(ImDrawList* list, ImFont* font, float size, ImVec2 min, ImVec2 max, ImU32 color, const char* text) {
            const auto extent = font->CalcTextSizeA(size, FLT_MAX, 0.0F, text);
            list->AddText(font, size, {(min.x + max.x - extent.x) / 2, (min.y + max.y - extent.y) / 2}, color, text);
        }
    }

    void DrawPadKeyboard(const core::PadKeyboard& keyboard, float scale) {
        const auto& io = ImGui::GetIO();
        const auto rows = core::PadKeyboard::Rows();
        const float gap = 6.0F * scale, pad = 14.0F * scale, rounding = 2.0F * scale;
        const float keyW = std::min(64.0F * scale, (io.DisplaySize.x * 0.9F - 2 * pad) / core::PadKeyboard::kColumns - gap);
        const float keyH = keyW * 0.78F;
        const float fontSize = keyH * 0.42F;
        const float hintH = fontSize * 1.8F;
        const ImVec2 size{core::PadKeyboard::kColumns * (keyW + gap) - gap + 2 * pad,
                          static_cast<float>(rows.size()) * (keyH + gap) - gap + 2 * pad + hintH};
        // Stay clear of the field being typed into: dock to whichever screen half it is not in.
        const float fieldY = GImGui->PlatformImeData.InputPos.y;
        const float margin = 24.0F * scale;
        const ImVec2 origin{(io.DisplaySize.x - size.x) / 2,
                            fieldY > io.DisplaySize.y / 2 ? margin : io.DisplaySize.y - size.y - margin};

        auto* list = ImGui::GetForegroundDrawList();
        auto* font = ImGui::GetFont();
        // FO4 menu look in the player's HUD color: black panel, HUD lines, the selection a solid HUD bar.
        list->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(0, 0, 0, 245), rounding);
        list->AddRect(origin, {origin.x + size.x, origin.y + size.y}, Hud(200), rounding, 0, 1.5F * scale);

        for (std::size_t r = 0; r < rows.size(); ++r) {
            const auto row = rows[r];
            for (std::size_t i = 0; i < row.size(); ++i) {
                const auto& key = row[i];
                const int start = core::PadKeyboard::KeyStart(row, i);
                const ImVec2 min{origin.x + pad + static_cast<float>(start) * (keyW + gap),
                                 origin.y + pad + static_cast<float>(r) * (keyH + gap)};
                const ImVec2 max{min.x + key.span * (keyW + gap) - gap, min.y + keyH};
                const bool focused = &key == &keyboard.Focused();
                const bool lit = key.action == core::PadKeyAction::Shift && keyboard.Shifted();
                list->AddRectFilled(min, max, IM_COL32(0, 0, 0, 255), rounding);
                list->AddRectFilled(min, max, focused ? Hud() : Hud(lit ? 90 : 26), rounding);
                if (!focused) list->AddRect(min, max, Hud(90), rounding, 0, scale);
                const char text[2]{keyboard.Shifted() ? key.shifted : key.normal, 0};
                CenteredText(list, font, fontSize, min, max, focused ? IM_COL32(0, 0, 0, 255) : Hud(), key.label ? key.label : text);
            }
        }
        const ImVec2 hintMin{origin.x, origin.y + size.y - pad - hintH};
        CenteredText(list, font, fontSize * 0.9F, hintMin, {origin.x + size.x, hintMin.y + hintH}, Hud(170),
                     "A type    X delete    Y space    LT shift    LB/RB move cursor    B done");
    }
}
