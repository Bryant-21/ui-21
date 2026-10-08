#include "b21ui/fo4/Style.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cctype>
#include <vector>

namespace b21ui::fo4 {
    namespace {
        // Flash sizes are em sizes; ImGui sizes are ascent + descent, which for Roboto (2400 / 2048 units)
        // is 1.172 em.
        constexpr float kEmToImGui = 2400.0F / 2048.0F;
        // FO4's embedded Roboto Condensed (fonts_en.swf) has the TTF's glyph shapes and cap height but
        // tighter advances: its "Leather Chest Piece" is 7.413 em against the TTF's 7.892 (bold 7.484 /
        // 8.038). Glyphs keep their size and are placed at FO4's advances.
        constexpr float kAdvanceRegular = 7.413F / 7.892F, kAdvanceBold = 7.484F / 8.038F;
        float AdvanceScale(bool bold) { return bold ? kAdvanceBold : kAdvanceRegular; }
        constexpr float kStageW = 1280.0F, kStageH = 720.0F;

        struct Frame {
            float scale = 1.0F;
            ImVec2 origin{};
            ImDrawList* list{};
        };
        Frame& Current() { return common::ContextSlot<Frame>(); }

        ImFont* FontOf(bool bold) {
            const auto& fonts = common::CurrentFonts();
            return bold && fonts.bold ? fonts.bold : fonts.regular ? fonts.regular : ImGui::GetFont();
        }

        std::string Upper(std::string_view text) {
            std::string out(text);
            for (auto& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return out;
        }

        void Triangle(ImVec2 a, ImVec2 b, ImVec2 c, ImU32 color) { Canvas()->AddTriangleFilled(Stage(a.x, a.y), Stage(b.x, b.y), Stage(c.x, c.y), color); }

        void Rect(float x0, float y0, float x1, float y1, ImU32 color) { Canvas()->AddRectFilled(Stage(x0, y0), Stage(x1, y1), color); }
    }

    void InitContext() {
        // fo4 menus navigate themselves, as FO4's do; ImGui's nav would fight them for the same keys.
        ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);
        const auto apply = [](float) {
            auto& s = ImGui::GetStyle();
            s.WindowPadding = {0, 0};
            s.WindowBorderSize = 0.0F;
            s.WindowRounding = 0.0F;
            s.Colors[ImGuiCol_WindowBg] = {0, 0, 0, 0};
        };
        ImGui::GetStyle() = ImGuiStyle();
        apply(common::UiScale());
        common::SetStyleRebuild(apply);
    }

    float Px(float units) { return units * Current().scale; }

    ImVec2 Stage(float x, float y) {
        const auto& f = Current();
        return {f.origin.x + x * f.scale, f.origin.y + y * f.scale};
    }

    ImDrawList* Canvas() {
        auto& f = Current();
        return f.list ? f.list : ImGui::GetForegroundDrawList();
    }

    void BeginMenu(const char* id, float dim) {
        const auto& io = ImGui::GetIO();
        auto& f = Current();
        f.scale = std::min(io.DisplaySize.x / kStageW, io.DisplaySize.y / kStageH);
        f.origin = {(io.DisplaySize.x - kStageW * f.scale) / 2, (io.DisplaySize.y - kStageH * f.scale) / 2};
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin(id, nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse);
        f.list = ImGui::GetWindowDrawList();
        if (dim > 0.0F) f.list->AddRectFilled({0, 0}, io.DisplaySize, IM_COL32(0, 0, 0, static_cast<int>(dim * 255)));
    }

    void EndMenu() {
        Current().list = nullptr;
        ImGui::End();
    }

    ImU32 Fg(float alpha) {
        const auto hud = common::HudColor();
        return (hud & ~IM_COL32_A_MASK) | static_cast<ImU32>(std::clamp(alpha, 0.0F, 1.0F) * 255.0F + 0.5F) << IM_COL32_A_SHIFT;
    }

    float TextWidth(float size, std::string_view text, bool bold, float spacing) {
        auto* font = FontOf(bold);
        const float px = size * kEmToImGui;
        const float width = font->CalcTextSizeA(px, FLT_MAX, 0.0F, text.data(), text.data() + text.size()).x;
        int glyphs = 0;
        for (const char c : text)
            if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++glyphs;
        return width * AdvanceScale(bold) + spacing * static_cast<float>(std::max(0, glyphs - 1));
    }

    void Text(float x, float y, float size, std::string_view text, ImU32 color, bool bold, float spacing, Align align) {
        if (text.empty()) return;
        const float width = TextWidth(size, text, bold, spacing);
        if (align == Align::Center) x -= width / 2;
        else if (align == Align::Right) x -= width;
        auto* font = FontOf(bold);
        const float px = Px(size * kEmToImGui);
        auto* list = Canvas();
        const float advance = AdvanceScale(bold);
        // ImGui has neither letter spacing nor FO4's advances: place each UTF-8 glyph at the scaled width
        // of the text before it (one rounding, where per-glyph widths each round up a pixel) plus tracking.
        const char* begin = text.data();
        const char* end = begin + text.size();
        const auto origin = Stage(x, y);
        int index = 0;
        for (const char* p = begin; p < end; ++index) {
            const char* next = p + 1;
            while (next < end && (static_cast<unsigned char>(*next) & 0xC0) == 0x80) ++next;
            const float before = p == begin ? 0.0F : font->CalcTextSizeA(px, FLT_MAX, 0.0F, begin, p).x;
            list->AddText(font, px, {origin.x + before * advance + Px(spacing) * static_cast<float>(index), origin.y}, color, p, next);
            p = next;
        }
    }

    Nav ReadNav() {
        const auto held = [](std::initializer_list<ImGuiKey> keys) {
            return std::ranges::any_of(keys, [](ImGuiKey k) { return ImGui::IsKeyPressed(k, true); });
        };
        const auto once = [](std::initializer_list<ImGuiKey> keys) {
            return std::ranges::any_of(keys, [](ImGuiKey k) { return ImGui::IsKeyPressed(k, false); });
        };
        Nav nav;
        if (held({ImGuiKey_UpArrow, ImGuiKey_W, ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadLStickUp})) nav.dy -= 1;
        if (held({ImGuiKey_DownArrow, ImGuiKey_S, ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadLStickDown})) nav.dy += 1;
        if (held({ImGuiKey_LeftArrow, ImGuiKey_A, ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadLStickLeft})) nav.dx -= 1;
        if (held({ImGuiKey_RightArrow, ImGuiKey_D, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickRight})) nav.dx += 1;
        nav.accept = once({ImGuiKey_Enter, ImGuiKey_KeypadEnter, ImGuiKey_E, ImGuiKey_GamepadFaceDown});
        nav.cancel = once({ImGuiKey_Escape, ImGuiKey_Tab, ImGuiKey_GamepadFaceRight});
        return nav;
    }

    void Brackets(float x0, float x1, float topY, float bottomY, float gapFrom, float gapTo) {
        const auto fg = Fg();
        if (gapTo > gapFrom) {
            Rect(x0, topY - 1, gapFrom, topY + 1, fg);
            Rect(gapTo, topY - 1, x1, topY + 1, fg);
        } else {
            Rect(x0, topY - 1, x1, topY + 1, fg);
        }
        Rect(x0 - 1, topY - 1, x0 + 1, topY + 4, fg);
        Rect(x1 - 1, topY - 1, x1 + 1, topY + 4, fg);
        Rect(x0, bottomY - 1, x1, bottomY + 1, fg);
        Rect(x0 - 1, bottomY - 4, x0 + 1, bottomY + 1, fg);
        Rect(x1 - 1, bottomY - 4, x1 + 1, bottomY + 1, fg);
    }

    float ListHeader(float x, float y, std::string_view title, bool arrows) {
        const auto text = Upper(title);
        const float width = TextWidth(25.0F, text, true, 0.5F);
        Text(x + 19.0F, y + 3.0F, 25.0F, text, Fg(), true, 0.5F);
        if (arrows) {
            Triangle({x + 4.8F, y + 15.85F}, {x + 12.0F, y + 8.65F}, {x + 12.0F, y + 23.05F}, Fg());
            const float rx = x + 19.0F + width + 10.0F;
            Triangle({rx + 7.2F, y + 15.85F}, {rx, y + 8.65F}, {rx, y + 23.05F}, Fg());
        }
        return 19.0F + width + 10.0F + 14.3F;
    }

    void ScrollMarker(float x, float y, bool up) {
        constexpr float w = 11.85F, h = 4.6F, gap = 3.2F;
        auto* list = Canvas();
        for (int i = 0; i < 2; ++i) {
            const float top = y + static_cast<float>(i) * (h + gap);
            const ImVec2 points[3]{Stage(x, up ? top + h : top), Stage(x + w / 2, up ? top : top + h), Stage(x + w, up ? top + h : top)};
            list->AddPolyline(points, 3, Fg(), ImDrawFlags_None, Px(1.41F));
        }
    }

    int HintBar(std::span<const Hint> hints, float y) {
        if (hints.empty()) return -1;
        const bool pad = common::ActiveDevice() == B21UI_DEVICE_GAMEPAD;
        constexpr float kGlyphR = 9.0F;
        std::vector<float> keyWidths, widths;
        std::vector<std::string> labels, keys;
        float total = 0.0F;
        for (const auto& hint : hints) {
            keys.push_back(std::string(hint.key) + ")");
            labels.push_back(Upper(hint.label));
            const float keyW = pad ? common::PadGlyphWidth(Px(kGlyphR), hint.padButton) / Current().scale : TextWidth(20.0F, keys.back());
            keyWidths.push_back(keyW);
            widths.push_back(2.0F + keyW + 4.0F + TextWidth(18.0F, labels.back(), true) + 4.0F);
            total += widths.back();
        }
        total += 20.0F * static_cast<float>(hints.size() - 1);
        const float left = 640.0F - total / 2;
        // Filled "[" and "]" flush against the group, 6.55 x 32, 2 thick.
        Rect(left - 6.55F, y, left - 4.55F, y + 32.0F, Fg());
        Rect(left - 6.55F, y, left, y + 2.0F, Fg());
        Rect(left - 6.55F, y + 30.0F, left, y + 32.0F, Fg());
        const float right = left + total;
        Rect(right + 4.55F, y, right + 6.55F, y + 32.0F, Fg());
        Rect(right, y, right + 6.55F, y + 2.0F, Fg());
        Rect(right, y + 30.0F, right + 6.55F, y + 32.0F, Fg());

        int clicked = -1;
        float x = left;
        for (std::size_t i = 0; i < hints.size(); ++i) {
            const auto color = Fg(hints[i].enabled ? 1.0F : 0.5F);
            if (pad) common::PadGlyph(Canvas(), Stage(x + 2.0F + keyWidths[i] / 2, y + 16.0F), Px(kGlyphR), hints[i].padButton, color);
            else Text(x + 2.0F, y + 4.0F, 20.0F, keys[i], color);
            Text(x + 2.0F + keyWidths[i] + 4.0F, y + 7.25F, 18.0F, labels[i], color, true);
            if (hints[i].enabled && ImGui::IsMouseHoveringRect(Stage(x, y), Stage(x + widths[i], y + 32.0F)) &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                clicked = static_cast<int>(i);
            x += widths[i] + 20.0F;
        }
        return clicked;
    }

    int MessageBox(std::string_view body, std::span<const char* const> buttons, int& selected, int cancelIndex, const Nav& nav) {
        constexpr float kWidth = 543.9F, kBodyWidth = 320.0F, kBodySize = 18.0F, kLine = 21.6F;
        constexpr float kButtonW = 450.0F, kButtonH = 30.0F, kButtonGap = 7.5F;
        // Word-wrap the body to FO4's 320-unit field.
        std::vector<std::string> lines{""};
        std::string_view rest = body;
        while (!rest.empty()) {
            const auto space = rest.find(' ');
            const auto word = rest.substr(0, space);
            const auto candidate = lines.back().empty() ? std::string(word) : lines.back() + " " + std::string(word);
            if (!lines.back().empty() && TextWidth(kBodySize, candidate) > kBodyWidth) lines.emplace_back(word);
            else lines.back() = candidate;
            rest = space == std::string_view::npos ? std::string_view{} : rest.substr(space + 1);
        }
        const int count = std::min<int>(static_cast<int>(buttons.size()), 5);
        const float listY = 18.0F + 4.0F + static_cast<float>(lines.size()) * kLine + 30.0F;
        const float buttonsH = static_cast<float>(count) * kButtonH + static_cast<float>(std::max(0, count - 1)) * kButtonGap;
        const float height = listY + buttonsH + 16.65F;
        const float left = 640.0F - kWidth / 2, top = 367.0F - height / 2;

        Canvas()->AddRectFilled(Stage(left, top), Stage(left + kWidth, top + height), IM_COL32(0, 0, 0, 229));
        Canvas()->AddRect(Stage(left, top), Stage(left + kWidth, top + height), Fg(), 0.0F, 0, Px(2.2F));
        for (std::size_t i = 0; i < lines.size(); ++i)
            Text(640.0F, top + 20.0F + static_cast<float>(i) * kLine, kBodySize, lines[i], Fg(), false, 0.0F, Align::Center);

        selected = std::clamp(selected, 0, std::max(0, count - 1));
        if (nav.dy != 0 && count > 0) selected = (selected + nav.dy + count) % count;
        int chosen = nav.accept ? selected : nav.cancel ? cancelIndex : -1;
        const bool moved = ImGui::GetIO().MouseDelta.x != 0.0F || ImGui::GetIO().MouseDelta.y != 0.0F;
        for (int i = 0; i < count; ++i) {
            const float bx = 640.0F - kButtonW / 2, by = top + listY + static_cast<float>(i) * (kButtonH + kButtonGap);
            if (ImGui::IsMouseHoveringRect(Stage(bx, by), Stage(bx + kButtonW, by + kButtonH))) {
                if (moved) selected = i;
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) chosen = i;
            }
            const bool on = i == selected;
            if (on) Rect(bx, by, bx + kButtonW, by + kButtonH, Fg());
            Text(640.0F, by + 6.2F, 18.0F, Upper(buttons[static_cast<std::size_t>(i)]), on ? Black : Fg(), true, -0.5F, Align::Center);
        }
        return chosen;
    }
}
