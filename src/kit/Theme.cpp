#include "b21ui/fo76/Style.h"
#include "kit/StyleInternal.h"

namespace b21ui::fo76::detail {
    void ApplyStyle() {
        auto& s = ImGui::GetStyle();
        s.WindowRounding = s.FrameRounding = s.PopupRounding = s.ScrollbarRounding = s.GrabRounding = s.TabRounding = 0.0F;
        s.WindowBorderSize = 0.0F;
        s.FrameBorderSize = 0.0F;
        s.WindowPadding = {16, 14};
        s.FramePadding = {10, 6};
        s.ItemSpacing = {10, 8};
        ApplyColors();
    }

    void ApplyColors() {
        auto* c = ImGui::GetStyle().Colors;
        const auto v = [](ImU32 col) { return ImGui::ColorConvertU32ToFloat4(col); };
        c[ImGuiCol_Text] = v(theme::Ink);
        c[ImGuiCol_TextDisabled] = v(theme::Disabled);
        c[ImGuiCol_WindowBg] = c[ImGuiCol_PopupBg] = c[ImGuiCol_ChildBg] = v(theme::Panel);
        c[ImGuiCol_ChildBg].w = 0.0F;
        c[ImGuiCol_Border] = c[ImGuiCol_Separator] = v(theme::Edge);
        c[ImGuiCol_FrameBg] = v(IM_COL32(255, 255, 203, 26));
        c[ImGuiCol_FrameBgHovered] = c[ImGuiCol_FrameBgActive] = v(IM_COL32(255, 255, 203, 51));
        c[ImGuiCol_Button] = v(IM_COL32(0, 0, 0, 0));
        c[ImGuiCol_ButtonHovered] = c[ImGuiCol_ButtonActive] = v(Accent());
        c[ImGuiCol_Header] = v(Accent(1.0F, 60));
        c[ImGuiCol_HeaderHovered] = c[ImGuiCol_HeaderActive] = v(Accent());
        c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = c[ImGuiCol_SliderGrabActive] = v(Accent());
        c[ImGuiCol_NavCursor] = v(Accent());
        c[ImGuiCol_ScrollbarBg] = v(IM_COL32(0, 0, 0, 0));
        c[ImGuiCol_ScrollbarGrab] = v(theme::Edge);
        c[ImGuiCol_ScrollbarGrabHovered] = c[ImGuiCol_ScrollbarGrabActive] = v(Accent());
        c[ImGuiCol_ModalWindowDimBg] = v(IM_COL32(0, 0, 0, 140));
    }
}
