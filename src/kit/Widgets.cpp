#include "b21ui/fo76/Style.h"

#include <imgui_internal.h>

#include <algorithm>

namespace b21ui::fo76 {
    using common::ActiveDevice;
    using common::ItemNavFocused;
    using common::kNoPad;
    using common::kPadGlyphOnly;
    using common::PadGlyph;
    using common::PadGlyphWidth;

    bool Button(const char* label, ImVec2 size, bool enabled, std::uint32_t padButton) {
        const bool pressable = padButton == kNoPad || !(padButton & kPadGlyphOnly);
        if (padButton != kNoPad) padButton &= ~kPadGlyphOnly;
        const auto text = ImGui::CalcTextSize(label, nullptr, true);
        const auto& style = ImGui::GetStyle();
        const bool glyph = padButton != kNoPad && ActiveDevice() == B21UI_DEVICE_GAMEPAD;
        const float r = text.y * 0.55F;
        const float glyphWidth = glyph ? PadGlyphWidth(r, padButton) + style.ItemInnerSpacing.x : 0.0F;
        if (size.x <= 0) size.x = text.x + glyphWidth + style.FramePadding.x * 2;
        if (size.y <= 0) size.y = text.y + style.FramePadding.y * 2;
        if (!enabled) ImGui::BeginDisabled();
        const auto pos = ImGui::GetCursorScreenPos();
        bool pressed = ImGui::InvisibleButton(label, size, ImGuiButtonFlags_EnableNav);
        const bool hot = enabled && (ImGui::IsItemHovered() || ItemNavFocused() || ImGui::IsItemActive());
        auto* list = ImGui::GetWindowDrawList();
        if (hot) list->AddRectFilled(pos, {pos.x + size.x, pos.y + size.y}, Accent());
        const ImU32 color = !enabled ? theme::Disabled : hot ? theme::DarkOnGold : ImGui::GetColorU32(ImGuiCol_Text);
        const float left = pos.x + style.FramePadding.x +
                           (size.x - style.FramePadding.x * 2 - text.x - glyphWidth) * style.ButtonTextAlign.x;
        if (glyph) PadGlyph(list, {left + glyphWidth * 0.5F - style.ItemInnerSpacing.x * 0.5F, pos.y + size.y * 0.5F}, r, padButton);
        list->AddText({left + glyphWidth, pos.y + (size.y - text.y) * 0.5F}, color, label, ImGui::FindRenderedTextEnd(label));
        if (!enabled) ImGui::EndDisabled();
        if (glyph && enabled && pressable && common::PadPressed(padButton)) pressed = true;
        return pressed && enabled;
    }

    bool ToggleButton(const char* label, bool on, ImVec2 size) {
        const auto pos = ImGui::GetCursorScreenPos();
        const bool pressed = Button(label, size);
        if (on) {
            const auto max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled({pos.x, max.y - 3}, max, Accent());
        }
        return pressed;
    }

    bool SliderFloat(const char* label, float* value, float min, float max, float step, const char* format) {
        const bool changed = ImGui::SliderFloat(label, value, min, max, format);
        return common::NavAdjust(value, min, max, step) || changed;
    }

    bool BeginPanel(const char* id, ImVec2 pos, ImVec2 size, ImGuiWindowFlags extra) {
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);
        const bool open = ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | extra);
        const auto p = ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, {p.x + ImGui::GetWindowWidth(), p.y + 3}, Accent());
        return open;
    }

    void EndPanel() { ImGui::End(); }

    void Heading(const char* text) {
        ImGui::PushFont(common::CurrentFonts().bold, theme::HeadingSize * TextScale());
        ImGui::TextUnformatted(text);
        ImGui::PopFont();
    }

    void Tooltip(const char* line1, const char* line2) {
        ImGui::BeginTooltip();
        ImGui::PushFont(common::CurrentFonts().bold, theme::TooltipSize * TextScale());
        ImGui::TextUnformatted(line1);
        ImGui::PopFont();
        if (line2) ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::Secondary), "%s", line2);
        ImGui::EndTooltip();
    }

    void PromptBar(std::span<const Prompt> prompts, std::uint32_t device, ImVec2 anchor) {
        auto* list = ImGui::GetForegroundDrawList();
        const float k = TextScale();
        const float r = 12.0F * k;
        float x = anchor.x;
        for (auto it = prompts.rbegin(); it != prompts.rend(); ++it) {
            const auto labelSize = ImGui::CalcTextSize(it->label);
            x -= labelSize.x;
            list->AddText({x, anchor.y - labelSize.y}, ImGui::GetColorU32(ImGuiCol_Text), it->label);
            x -= 8.0F * k;
            if (device == B21UI_DEVICE_GAMEPAD) {
                const float w = PadGlyphWidth(r, it->padButton) * 0.5F;
                x -= w;
                PadGlyph(list, {x, anchor.y - labelSize.y * 0.5F}, r, it->padButton, ImGui::GetColorU32(ImGuiCol_Text));
                x -= w;
            } else {
                const auto key = ImGui::CalcTextSize(it->keyLabel);
                const ImVec2 min{x - key.x - 12.0F * k, anchor.y - labelSize.y - 2.0F * k};
                list->AddRect(min, {x, anchor.y + 2.0F * k}, ImGui::GetColorU32(ImGuiCol_Border));
                list->AddText({min.x + 6.0F * k, anchor.y - labelSize.y}, ImGui::GetColorU32(ImGuiCol_Text), it->keyLabel);
                x = min.x;
            }
            x -= 22.0F * k;
        }
    }
}

