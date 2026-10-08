#include "b21ui/modern/Theme.h"

#include "b21ui/Common.h"
#include "b21ui/fo76/Style.h"

#include <algorithm>
#include <array>

namespace b21ui::modern::theme {
    namespace {
        struct ContextTheme {
            Fonts fonts;
            Fonts condensed;  // the Fallout 4 / Fallout 76 families
            float displayScale = 0.0F;
            float scale = 0.0F;
            float backgroundOpacity = -1.0F;
            float appliedWindowRounding = -1.0F;
            int accentRevision = -1;
            Family family = Family::Modern;
        };

        int accentRevision = 0;
        Family paletteFamily = Family::Modern;
        ImU32 paletteHud = 0;
        bool paletteSet = false;

        ContextTheme& Current() { return common::ContextSlot<ContextTheme>(); }

        // Font Awesome icons plus the arrows, stars and box glyphs Roboto lacks.
        // Font Awesome Solid and Brands share one code point space, so both merge into the text fonts.
        ImFont* Load(const std::filesystem::path& file, const std::filesystem::path& iconDir) {
            const std::array merge{common::FontMerge{iconDir / "Font_Awesome_7_Free-Solid-900.otf", {0.0F, 0.5F}},
                                   common::FontMerge{iconDir / "Font_Awesome_7_Brands-Regular-400.otf", {0.0F, 0.5F}},
                                   common::FontMerge{common::WindowsFont(L"seguisym.ttf")}};
            return common::LoadFont(file, iconDir.empty() ? std::span(merge).subspan(2) : std::span(merge));
        }

        ImVec4 Blend(ImU32 a, ImU32 b, float weight) {
            const auto x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
            return {x.x + (y.x - x.x) * weight, x.y + (y.y - x.y) * weight, x.z + (y.z - x.z) * weight, 1.0F};
        }

        ImU32 Mix(ImU32 a, ImU32 b, float weight) { return ImGui::ColorConvertFloat4ToU32(Blend(a, b, weight)); }

        ImU32 WithAlpha(ImU32 color, int alpha) {
            return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
        }

        // Dark picks are brightened so accent text stays readable on the dark surfaces.
        ImU32 Readable(float r, float g, float b) {
            float h{}, s{}, v{};
            ImGui::ColorConvertRGBtoHSV(std::clamp(r, 0.0F, 1.0F), std::clamp(g, 0.0F, 1.0F), std::clamp(b, 0.0F, 1.0F), h, s, v);
            v = std::max(v, 0.78F);
            ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
            // Saturated blues and reds stay dim at full value; lift them toward white until accent text reads on #101114.
            const auto luma = [&] { return 0.2126F * r + 0.7152F * g + 0.0722F * b; };
            for (int step = 0; step < 8 && luma() < 0.38F; ++step) {
                r += (1 - r) * 0.15F;
                g += (1 - g) * 0.15F;
                b += (1 - b) * 0.15F;
            }
            return ImGui::ColorConvertFloat4ToU32({r, g, b, 1.0F});
        }

        void AssignAccent(ImU32 accent, ImU32 onAccent) {
            constexpr ImU32 black = IM_COL32(0, 0, 0, 0xFF), white = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
            Accent = accent;
            AccentMuted = Mix(black, accent, 0.77F);
            AccentHover = Mix(accent, white, 0.2F);
            AccentSoft = WithAlpha(accent, 0x2E);
            OnAccent = onAccent;
        }

        void Statuses(ImU32 ok, ImU32 warn, ImU32 error) {
            Ok = ok;
            Warn = warn;
            Error = error;
            OkFill = WithAlpha(ok, 0x22);
            WarnFill = WithAlpha(warn, 0x22);
            ErrorFill = WithAlpha(error, 0x26);
        }

        void Palette(Family family, ImU32 hud) {
            constexpr ImU32 black = IM_COL32(0, 0, 0, 0xFF);
            const auto hudVec = ImGui::ColorConvertU32ToFloat4(hud);
            const auto readableHud = Readable(hudVec.x, hudVec.y, hudVec.z);
            Scrim = IM_COL32(0x00, 0x00, 0x00, 0x73);
            switch (family) {
            case Family::Fallout4:
                // FO4 menus: the HUD colour on black, everything else a shade of it (fo4::Fg).
                Background = Mix(black, readableHud, 0.04F);
                Sidebar = Mix(black, readableHud, 0.07F);
                Surface = Mix(black, readableHud, 0.11F);
                Input = Mix(black, readableHud, 0.16F);
                Border = Mix(black, readableHud, 0.45F);
                Text = readableHud;
                Muted = Mix(black, readableHud, 0.78F);
                Faint = Mix(black, readableHud, 0.52F);
                AssignAccent(readableHud, IM_COL32(0, 0, 0, 0xFF));
                AccentSoft = WithAlpha(readableHud, 0x40);
                Statuses(readableHud, IM_COL32(0xED, 0xC5, 0x75, 0xFF), IM_COL32(0xF2, 0x8A, 0x91, 0xFF));
                break;
            case Family::Fallout76: {
                const ImU32 panel = WithAlpha(fo76::theme::Panel, 0xFF);
                Background = panel;
                Sidebar = Mix(panel, black, 0.25F);
                Surface = Mix(panel, fo76::theme::Ink, 0.07F);
                Input = Mix(panel, fo76::theme::Ink, 0.12F);
                Border = Mix(panel, fo76::theme::Ink, 0.38F);
                Text = fo76::theme::Ink;
                Muted = fo76::theme::Secondary;
                Faint = fo76::theme::Disabled;
                AssignAccent(fo76::theme::Gold, fo76::theme::DarkOnGold);
                Statuses(IM_COL32(0x79, 0xCD, 0x92, 0xFF), fo76::theme::Warning, fo76::theme::Fail);
                break;
            }
            default:
                Background = IM_COL32(0x10, 0x11, 0x14, 0xFF);
                Sidebar = IM_COL32(0x17, 0x18, 0x1C, 0xFF);
                Surface = IM_COL32(0x22, 0x24, 0x29, 0xFF);
                Input = IM_COL32(0x2B, 0x2E, 0x34, 0xFF);
                Border = IM_COL32(0x3A, 0x3E, 0x46, 0xFF);
                Text = IM_COL32(0xEC, 0xEE, 0xF2, 0xFF);
                Muted = IM_COL32(0xA6, 0xAE, 0xBA, 0xFF);
                Faint = IM_COL32(0x6E, 0x76, 0x82, 0xFF);
                // After the lift every accent is light enough that near-black text on it out-contrasts white.
                AssignAccent(readableHud, IM_COL32(0x14, 0x12, 0x0A, 0xFF));
                Statuses(IM_COL32(0x79, 0xCD, 0x92, 0xFF), IM_COL32(0xED, 0xC5, 0x75, 0xFF), IM_COL32(0xF2, 0x8A, 0x91, 0xFF));
                break;
            }
        }

        void Colors(float backgroundOpacity) {
            auto* c = ImGui::GetStyle().Colors;
            const auto hover = Blend(Input, Accent, 0.28F);
            c[ImGuiCol_WindowBg] = Vec(Background);
            c[ImGuiCol_WindowBg].w *= backgroundOpacity;
            c[ImGuiCol_ChildBg] = Vec(IM_COL32(0, 0, 0, 0));
            c[ImGuiCol_PopupBg] = Vec(Surface);
            c[ImGuiCol_Text] = Vec(Text);
            c[ImGuiCol_TextDisabled] = Vec(Muted);
            c[ImGuiCol_Border] = Vec(Border);
            c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
            c[ImGuiCol_Separator] = Vec(Border);
            c[ImGuiCol_SeparatorHovered] = Vec(AccentMuted);
            c[ImGuiCol_SeparatorActive] = Vec(Accent);
            c[ImGuiCol_FrameBg] = Vec(Input);
            c[ImGuiCol_FrameBgHovered] = Blend(Input, Accent, 0.12F);
            c[ImGuiCol_FrameBgActive] = Blend(Input, Accent, 0.18F);
            c[ImGuiCol_CheckMark] = Vec(Accent);
            c[ImGuiCol_SliderGrab] = Vec(AccentMuted);
            c[ImGuiCol_SliderGrabActive] = Vec(Accent);
            c[ImGuiCol_Button] = Vec(Input);
            c[ImGuiCol_ButtonHovered] = hover;
            c[ImGuiCol_ButtonActive] = Vec(AccentMuted);
            c[ImGuiCol_Header] = Blend(Surface, Accent, 0.22F);
            c[ImGuiCol_HeaderHovered] = Blend(Surface, Accent, 0.12F);
            c[ImGuiCol_HeaderActive] = Blend(Surface, Accent, 0.30F);
            c[ImGuiCol_TitleBg] = Vec(Background);
            c[ImGuiCol_TitleBgActive] = Vec(Surface);
            c[ImGuiCol_MenuBarBg] = Vec(Background);
            c[ImGuiCol_Tab] = Vec(Surface);
            c[ImGuiCol_TabSelected] = Vec(AccentMuted);
            c[ImGuiCol_TabHovered] = hover;
            c[ImGuiCol_TabSelectedOverline] = Vec(Accent);
            c[ImGuiCol_TableHeaderBg] = Vec(Input);
            c[ImGuiCol_TableBorderStrong] = Vec(Border);
            c[ImGuiCol_TableBorderLight] = Blend(Surface, Border, 0.6F);
            c[ImGuiCol_TableRowBg] = {0, 0, 0, 0};
            c[ImGuiCol_TableRowBgAlt] = {1, 1, 1, 0.025F};
            c[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
            c[ImGuiCol_ScrollbarGrab] = Vec(Border);
            c[ImGuiCol_ScrollbarGrabHovered] = Vec(Muted);
            c[ImGuiCol_ScrollbarGrabActive] = Vec(Accent);
            c[ImGuiCol_PlotLines] = Vec(Accent);
            c[ImGuiCol_PlotLinesHovered] = Vec(Warn);
            c[ImGuiCol_PlotHistogram] = Vec(AccentMuted);
            c[ImGuiCol_PlotHistogramHovered] = Vec(Accent);
            c[ImGuiCol_TextSelectedBg] = Blend(Input, Accent, 0.45F);
            c[ImGuiCol_NavCursor] = Vec(Accent);
            c[ImGuiCol_ResizeGrip] = {0, 0, 0, 0};
            c[ImGuiCol_ModalWindowDimBg] = Vec(Scrim);
        }

        void Metrics(float scale, Family family) {
            auto& s = ImGui::GetStyle();
            s.WindowPadding = {16, 16};
            s.FramePadding = {10, 6};
            s.ItemSpacing = {10, 8};
            s.ItemInnerSpacing = {6, 5};
            s.CellPadding = {8, 6};
            s.WindowRounding = 10;
            s.ChildRounding = 6;
            s.FrameRounding = 5;
            s.PopupRounding = 6;
            s.GrabRounding = 4;
            s.TabRounding = 4;
            s.ScrollbarRounding = 6;
            s.ScrollbarSize = 12;
            s.GrabMinSize = 10;
            s.WindowBorderSize = 1;
            s.ChildBorderSize = 1;
            s.PopupBorderSize = 1;
            s.FrameBorderSize = 1;
            s.TabBarOverlineSize = 0;
            s.SeparatorTextBorderSize = 1;
            s.SeparatorTextPadding = {0, 4};
            s.SelectableTextAlign = {0, 0.5F};
            s.ButtonTextAlign = {0.5F, 0.5F};
            s.WindowTitleAlign = {0, 0.5F};
            s.IndentSpacing = 18;
            if (family != Family::Modern) {
                // Both games draw square frames; a hair of window rounding marks the style as ours (see Apply).
                s.WindowRounding = 0.01F;
                s.ChildRounding = s.FrameRounding = s.PopupRounding = s.GrabRounding = s.TabRounding = s.ScrollbarRounding = 0;
            }
            s.ScaleAllSizes(scale);
            s.FontSizeBase = BodySize * scale;
        }

        void UpdatePalette(Family family) {
            const auto hud = common::HudColor();
            if (paletteSet && paletteFamily == family && paletteHud == hud) return;
            Palette(family, hud);
            paletteSet = true;
            paletteFamily = family;
            paletteHud = hud;
            ++accentRevision;
        }
    }

    ImVec4 Vec(ImU32 color) { return ImGui::ColorConvertU32ToFloat4(color); }

    void LoadFonts(const std::filesystem::path& dir) {
        auto& current = Current();
        auto& fonts = current.fonts;
        fonts.body = Load(dir / "Roboto-Regular.ttf", dir);
        fonts.bold = Load(dir / "Roboto-Bold.ttf", dir);
        fonts.mono = Load(dir / "Inconsolata-Medium.ttf", {});
        const auto& kit = common::CurrentFonts();
        if (!fonts.body) fonts.body = kit.regular ? kit.regular : ImGui::GetIO().Fonts->AddFontDefault();
        if (!fonts.bold) fonts.bold = kit.bold ? kit.bold : fonts.body;
        if (!fonts.mono) fonts.mono = fonts.body;
        auto& condensed = current.condensed;
        condensed.body = Load(dir / "RobotoCondensed-Regular.ttf", dir);
        condensed.bold = Load(dir / "RobotoCondensed-Bold.ttf", dir);
        condensed.mono = fonts.mono;
        if (!condensed.body) condensed.body = fonts.body;
        if (!condensed.bold) condensed.bold = fonts.bold;
        // Modern is this context's style from now on: scale changes rebuild it instead of the fo76 default.
        common::SetStyleRebuild([](float scale) { Apply(scale); });
    }

    const Fonts& CurrentFonts() {
        const auto& current = Current();
        return CurrentAppearance().family == Family::Modern || !current.condensed.body ? current.fonts : current.condensed;
    }

    float Scale() {
        const auto scale = Current().scale;
        return scale > 0.0F ? scale : 1.0F;
    }

    float Round(float value) { return CurrentAppearance().family == Family::Modern ? Px(value) : 0.0F; }

    void Apply(float displayScale) {
        const auto& appearance = CurrentAppearance();
        UpdatePalette(appearance.family);
        auto& current = Current();
        auto& style = ImGui::GetStyle();
        const float scale = displayScale * appearance.textScale;
        // The kit rebuilds its own style whenever the display scale changes; our rounding marks ours.
        if (current.displayScale == displayScale && current.scale == scale && current.backgroundOpacity == appearance.backgroundOpacity &&
            style.WindowRounding == current.appliedWindowRounding && current.accentRevision == accentRevision &&
            current.family == appearance.family)
            return;
        style = ImGuiStyle();
        Metrics(scale, appearance.family);
        Colors(appearance.backgroundOpacity);
        current.displayScale = displayScale;
        current.scale = scale;
        current.backgroundOpacity = appearance.backgroundOpacity;
        current.appliedWindowRounding = style.WindowRounding;
        current.accentRevision = accentRevision;
        current.family = appearance.family;
    }

    void SetAccent(float r, float g, float b) {
        const auto accent = Readable(r, g, b);
        if (accent == Accent) return;
        AssignAccent(accent, OnAccent);
        ++accentRevision;
    }
}
