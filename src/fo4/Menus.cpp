#include "b21ui/fo4/Style.h"

#include <algorithm>
#include <format>

namespace b21ui::fo4 {
    namespace {
        void Rect(float x0, float y0, float x1, float y1, ImU32 color) { Canvas()->AddRectFilled(Stage(x0, y0), Stage(x1, y1), color); }

        void Triangle(ImVec2 a, ImVec2 b, ImVec2 c, ImU32 color) {
            Canvas()->AddTriangleFilled(Stage(a.x, a.y), Stage(b.x, b.y), Stage(c.x, c.y), color);
        }

        void Quad(ImVec2 a, ImVec2 b, ImVec2 c, ImVec2 d, ImU32 color) {
            Canvas()->AddQuadFilled(Stage(a.x, a.y), Stage(b.x, b.y), Stage(c.x, c.y), Stage(d.x, d.y), color);
        }

        bool Hovered(float x0, float y0, float x1, float y1) { return ImGui::IsMouseHoveringRect(Stage(x0, y0), Stage(x1, y1)); }

        bool MouseMoved() {
            const auto d = ImGui::GetIO().MouseDelta;
            return d.x != 0.0F || d.y != 0.0F;
        }

        // Measured container-menu geometry (docs/fo4-style-metrics.md section 5).
        struct PanelGeometry {
            float frameX0, frameX1, topBar, bottomBar;
            float headerX, headerY;
            float firstRow, pitch;
            int rows;
            float textX, textDy;          // glyph box relative to the row
            float bodyX0, bodyX1, bodyDy, bodyH;
            bool pointsRight;             // selection arrow toward the screen centre
            float scrollX, scrollUpY, scrollDownY;
        };
        constexpr PanelGeometry kPlayer{185.35F, 506.5F, 195.75F, 521.5F, 196.5F, 171.25F, 223.0F, 26.1F, 9,
                                        210.75F, 5.0F, 184.0F, 504.9F, 0.5F, 26.1F, true, 214.0F, 214.5F, 471.5F};
        constexpr PanelGeometry kOther{774.15F, 1095.3F, 195.4F, 521.1F, 787.25F, 170.9F, 223.5F, 26.2F, 10,
                                       798.7F, 4.0F, 775.4F, 1094.45F, 1.0F, 25.9F, false, 803.5F, 212.9F, 499.0F};

        // Solid bar with an arrow point and a detached chevron, pointing toward the screen centre.
        void SelectionBar(const PanelGeometry& g, float rowY) {
            const float top = rowY + g.bodyDy, bottom = top + g.bodyH, mid = (top + bottom) / 2;
            const float dir = g.pointsRight ? 1.0F : -1.0F;
            const float edge = g.pointsRight ? g.bodyX1 : g.bodyX0;
            Rect(g.bodyX0, top, g.bodyX1, bottom, Fg());
            const float tip = edge + dir * 8.5F;
            Triangle({edge, top}, {tip, mid}, {edge, bottom}, Fg());
            // Chevron band: 3.81 thick, 3.69 clear of the body's point.
            const float inner = tip + dir * 3.69F, outer = inner + dir * 3.81F;
            const float back = inner - dir * 8.41F;
            Quad({back, top}, {back + dir * 3.81F, top}, {outer, mid}, {inner, mid}, Fg());
            Quad({inner, mid}, {outer, mid}, {back + dir * 3.81F, bottom}, {back, bottom}, Fg());
        }

        std::string ItemText(const Item& item) {
            return item.count != 1 ? std::format("{} ({})", item.name, item.count) : item.name;
        }

        void WeightIcon(float cx, float cy, ImU32 color) {
            auto* list = Canvas();
            list->AddCircle(Stage(cx, cy - 5.5F), Px(3.2F), color, 0, Px(1.8F));
            Quad({cx - 4.5F, cy - 3.0F}, {cx + 4.5F, cy - 3.0F}, {cx + 8.0F, cy + 8.5F}, {cx - 8.0F, cy + 8.5F}, color);
        }

        void CapsIcon(float cx, float cy, ImU32 color) {
            Canvas()->AddCircleFilled(Stage(cx, cy), Px(8.5F), color, 12);
            Text(cx - 3.6F, cy - 7.0F, 12.0F, "C", Black, true);
        }
    }

    ListResult InventoryPanel(Side side, std::string_view title, std::span<const Item> items, ListModel& model, bool focused,
                              const Nav& nav) {
        const auto& g = side == Side::Player ? kPlayer : kOther;
        ListResult result;
        model.SetVisibleRows(g.rows);
        model.SetCount(static_cast<int>(items.size()));

        const float headerWidth = ListHeader(g.headerX, g.headerY, title);
        Brackets(g.frameX0, g.frameX1, g.topBar, g.bottomBar, g.headerX, g.headerX + headerWidth);

        const int before = model.Selected();
        if (focused) {
            if (nav.dy != 0) model.Move(nav.dy, false);
            if (nav.accept && model.Selected() >= 0) result.activated = model.Selected();
        }
        if (Hovered(g.frameX0, g.topBar, g.frameX1, g.bottomBar)) {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0F) model.Move(wheel > 0 ? -1 : 1, false);
        }
        for (int i = model.First(); i < model.Last(); ++i) {
            const float rowY = g.firstRow + static_cast<float>(i - model.First()) * g.pitch;
            if (Hovered(g.bodyX0, rowY + g.bodyDy, g.bodyX1, rowY + g.bodyDy + g.bodyH)) {
                if (MouseMoved()) model.Select(i);
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    model.Select(i);
                    result.activated = i;
                }
            }
        }
        for (int i = model.First(); i < model.Last(); ++i) {
            const auto& item = items[static_cast<std::size_t>(i)];
            const float rowY = g.firstRow + static_cast<float>(i - model.First()) * g.pitch;
            // Only the focused panel shows its selection, as in FO4.
            const bool selected = focused && i == model.Selected();
            if (selected) SelectionBar(g, rowY);
            const ImU32 ink = selected ? Black : Fg(item.disabled ? 0.5F : 1.0F);
            const auto text = ItemText(item);
            Text(g.textX, rowY + g.textDy, 18.0F, text, ink, false, 0.5F);
            if (item.equipped) Rect(g.textX - 17.25F, rowY + g.bodyDy + 9.25F, g.textX - 9.25F, rowY + g.bodyDy + 17.25F, ink);
            float iconX = g.textX + TextWidth(18.0F, text, false, 0.5F) + 10.0F;
            if (item.legendary) {
                Text(iconX, rowY + g.textDy + 2.0F, 13.0F, "\xE2\x98\x85", ink);
                iconX += 17.0F;
            }
            if (item.favorite) Text(iconX, rowY + g.textDy + 2.0F, 12.0F, "\xE2\x99\xA5", ink);
        }
        if (model.First() > 0) ScrollMarker(g.scrollX, g.scrollUpY, true);
        if (model.Last() < model.Count()) ScrollMarker(g.scrollX, g.scrollDownY, false);
        result.selectionChanged = model.Selected() != before;
        return result;
    }

    void ItemCard(std::span<const Stat> rows) {
        constexpr float kRight = 722.0F, kWidth = 167.8F, kRowH = 23.1F, kPitch = 23.9F, kBottom = 484.0F, kSize = 19.6F;
        const float firstTop = kBottom - kRowH - static_cast<float>(rows.size() - 1) * kPitch;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const float top = firstTop + static_cast<float>(i) * kPitch;
            Rect(kRight - kWidth, top, kRight, top + kRowH, Fg(rows[i].strong ? 0.4F : 0.2F));
            Text(kRight - kWidth + 5.8F, top - 0.6F, kSize, rows[i].label, Fg());
            Text(kRight - 4.5F, top - 0.6F, kSize, rows[i].value, Fg(), true, 0.0F, Align::Right);
        }
    }

    void InventoryFooter(std::string_view weight, std::string_view caps) {
        constexpr float kY = 481.75F, kH = 29.65F;
        Rect(191.55F, kY, 343.55F, kY + kH, Fg(0.2F));
        Rect(345.05F, kY, 497.05F, kY + kH, Fg(0.2F));
        WeightIcon(210.4F, 497.2F, Fg());
        CapsIcon(361.2F, 496.7F, Fg());
        Text(228.4F, 484.05F, 20.7F, weight, Fg(), true);
        Text(378.2F, 484.05F, 20.7F, caps, Fg(), true);
    }

    int PauseList(std::span<const char* const> options, ListModel& model, bool focused, const Nav& nav) {
        constexpr float kX = 303.0F, kY = 302.0F, kW = 134.1F, kH = 28.0F, kPitch = 29.2F;
        Brackets(292.0F, 451.0F, 270.0F, 597.0F);
        model.SetVisibleRows(9);
        model.SetCount(static_cast<int>(options.size()));
        int activated = -1;
        if (focused) {
            if (nav.dy != 0) model.Move(nav.dy, true);
            if (nav.accept) activated = model.Selected();
        }
        for (int i = model.First(); i < model.Last(); ++i) {
            const float y = kY + static_cast<float>(i - model.First()) * kPitch;
            if (Hovered(kX, y, kX + kW, y + kH)) {
                if (MouseMoved()) model.Select(i);
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) activated = i;
            }
            const bool selected = i == model.Selected();
            if (selected) Rect(kX, y, kX + kW, y + kH, Fg());
            Text(kX + 14.0F, y + 4.6F, 19.0F, options[static_cast<std::size_t>(i)], selected ? Black : Fg(), false, 0.1F);
        }
        return activated;
    }

    namespace {
        constexpr float kRowX = 471.4F, kRowY = 325.0F, kRowW = 330.0F, kRowH = 25.95F, kRowPitch = 29.65F;
        float RowTop(int row) { return kRowY + static_cast<float>(row) * kRowPitch; }

        // Label and selection bar; returns the control origin (row x + 210, row y - 1).
        ImVec2 SettingRow(int row, std::string_view label, bool selected) {
            const float y = RowTop(row);
            if (selected) Rect(kRowX, y, kRowX + kRowW, y + kRowH, Fg());
            Text(kRowX + 14.0F, y + 3.0F, 19.0F, label, selected ? Black : Fg(), false, 0.2F);
            return {kRowX + 210.0F, y - 1.0F};
        }

        bool Clicked(float x0, float y0, float x1, float y1) { return Hovered(x0, y0, x1, y1) && ImGui::IsMouseClicked(ImGuiMouseButton_Left); }
    }

    void SettingsFrame() { Brackets(457.35F, 825.35F, 290.1F, 597.05F); }

    bool SettingRowHovered(int row) {
        const float y = RowTop(row);
        return MouseMoved() && Hovered(kRowX, y, kRowX + kRowW, y + kRowH);
    }

    bool SettingSlider(int row, std::string_view label, float& value, bool selected, const Nav& nav, float step) {
        const auto c = SettingRow(row, label, selected);
        const ImU32 ink = selected ? Black : Fg();
        const float trackX = c.x + 3.0F, trackY = c.y + 9.05F, trackW = 108.7F, trackH = 11.5F;
        const float before = value;
        if (selected && nav.dx != 0) value += static_cast<float>(nav.dx) * step;
        if (Clicked(c.x - 9.25F, trackY, trackX, trackY + trackH)) value -= step;
        if (Clicked(trackX + trackW, trackY, trackX + trackW + 9.25F, trackY + trackH)) value += step;
        if (Hovered(trackX, trackY - 3.0F, trackX + trackW, trackY + trackH + 3.0F) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const float mouse = (ImGui::GetIO().MousePos.x - Stage(trackX, 0).x) / Px(trackW);
            value = std::round(mouse / step) * step;
        }
        value = std::clamp(value, 0.0F, 1.0F);
        Canvas()->AddRect(Stage(trackX, trackY), Stage(trackX + trackW, trackY + trackH), ink, 0.0F, 0, Px(1.6F));
        const float thumbX = trackX + value * (trackW - 7.0F);
        Rect(thumbX, trackY, thumbX + 7.0F, trackY + trackH, ink);
        Triangle({c.x - 5.77F, trackY + trackH / 2}, {trackX, trackY}, {trackX, trackY + trackH}, ink);
        Triangle({c.x + 119.6F, trackY + trackH / 2}, {c.x + 110.85F, trackY}, {c.x + 110.85F, trackY + trackH}, ink);
        return value != before;
    }

    bool SettingToggle(int row, std::string_view label, bool& on, bool selected, const Nav& nav) {
        const auto c = SettingRow(row, label, selected);
        const float y = RowTop(row);
        bool changed = (selected && (nav.dx != 0 || nav.accept)) || Clicked(kRowX, y, kRowX + kRowW, y + kRowH);
        if (changed) on = !on;
        Text(c.x + 56.25F, c.y + 5.85F, 19.0F, on ? "ON" : "OFF", selected ? Black : Fg(), false, -0.5F, Align::Center);
        return changed;
    }

    bool SettingStepper(int row, std::string_view label, int& index, std::span<const char* const> values, bool selected,
                        const Nav& nav) {
        if (values.empty()) return false;
        const auto c = SettingRow(row, label, selected);
        const ImU32 ink = selected ? Black : Fg();
        const int count = static_cast<int>(values.size());
        const int before = index;
        const std::string_view text = values[static_cast<std::size_t>(std::clamp(index, 0, count - 1))];
        const float textW = TextWidth(19.0F, text, false, -0.5F);
        const float textX = c.x + 56.5F - textW / 2;
        const float arrowTop = c.y + 8.75F, arrowBottom = c.y + 20.75F, arrowMid = (arrowTop + arrowBottom) / 2;
        const float leftTip = textX - 8.0F, rightTip = textX + textW + 8.0F;
        int step = selected ? nav.dx : 0;
        if (Clicked(leftTip - 4.0F, arrowTop, textX, arrowBottom)) step = -1;
        if (Clicked(textX + textW, arrowTop, rightTip + 4.0F, arrowBottom)) step = 1;
        index = std::clamp(index + step, 0, count - 1);
        const std::string_view shown = values[static_cast<std::size_t>(index)];
        const float shownW = TextWidth(19.0F, shown, false, -0.5F);
        const float shownX = c.x + 56.5F - shownW / 2;
        Text(c.x + 56.5F, c.y + 5.6F, 19.0F, shown, ink, false, -0.5F, Align::Center);
        if (index > 0) Triangle({shownX - 8.0F, arrowMid}, {shownX - 2.0F, arrowTop}, {shownX - 2.0F, arrowBottom}, ink);
        if (index < count - 1) {
            const float r = shownX + shownW + 2.0F;
            Triangle({r + 6.0F, arrowMid}, {r, arrowTop}, {r, arrowBottom}, ink);
        }
        return index != before;
    }
}
