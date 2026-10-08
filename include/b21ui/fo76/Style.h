#pragma once
// The Fallout 76 map look: gold ink on dark translucent panels, Roboto Condensed. Used by the
// full-screen map (through the b21ui::kit compatibility name) and the B21UI demo.
#include "b21ui/Common.h"

namespace b21ui::fo76 {
    namespace theme {
        inline constexpr ImU32 Ink = IM_COL32(0xFF, 0xFF, 0xCB, 0xFF);
        inline constexpr ImU32 Gold = IM_COL32(0xF5, 0xCB, 0x5B, 0xFF);
        inline constexpr ImU32 Panel = IM_COL32(24, 28, 29, 209);
        inline constexpr ImU32 Edge = IM_COL32(255, 255, 203, 102);
        inline constexpr ImU32 DarkOnGold = IM_COL32(0x18, 0x1C, 0x1D, 0xFF);
        inline constexpr ImU32 Viewport = IM_COL32(0x31, 0x39, 0x3B, 0xFF);
        inline constexpr ImU32 Disabled = IM_COL32(0x89, 0x92, 0x94, 0xFF);
        inline constexpr ImU32 Secondary = IM_COL32(0xC1, 0xC6, 0xB5, 0xFF);
        inline constexpr ImU32 Hostile = IM_COL32(0xFF, 0x89, 0x7E, 0xFF);
        inline constexpr ImU32 Fail = IM_COL32(0xF6, 0x72, 0x59, 0xFF);
        inline constexpr ImU32 Warning = IM_COL32(0xFF, 0xB4, 0x54, 0xFF);
        inline constexpr ImU32 PadA = common::pad::A;
        inline constexpr ImU32 PadB = common::pad::B;
        inline constexpr ImU32 PadX = common::pad::X;
        inline constexpr ImU32 PadY = common::pad::Y;
        inline constexpr float TitleSize = 30.0F, HeadingSize = 26.0F, ButtonSize = 18.0F, TooltipSize = 22.0F,
                               MenuSize = 19.0F, RowSize = 20.0F;
    }

    using Fonts = common::Fonts;

    // common::InitBaseContext plus the fo76 style, which it registers as this context's style.
    // `textScale` multiplies every font size.
    Fonts InitContext(const std::filesystem::path& fontDir, float textScale = 1.0F);
    // Effective multiplier for fo76 sizes: user text scale times the resolution scale.
    float TextScale();
    void SetTextScale(float scale);
    // The highlight color every widget uses (hover fill, focus, panel rule): theme::Gold until the client
    // sets one, e.g. the map's Pip-Boy color. The second form scales its brightness and sets its alpha.
    ImU32 Accent();
    ImU32 Accent(float brightness, int alpha = 255);
    void SetAccent(ImU32 color);

    // Square, transparent at rest, gold fill + dark text when hovered or nav-focused. With a
    // `padButton` (B21UI_PAD_*), a controller user sees that button's glyph inside it and can press it
    // directly while the window is focused. The label follows style.ButtonTextAlign.x.
    bool Button(const char* label, ImVec2 size = {0, 0}, bool enabled = true, std::uint32_t padButton = common::kNoPad);
    bool ToggleButton(const char* label, bool on, ImVec2 size = {0, 0});
    // ImGui slider that a controller can change while it is only focused (common::NavAdjust).
    bool SliderFloat(const char* label, float* value, float min, float max, float step = 0.05F, const char* format = "%.2f");
    // Dark translucent panel with a 3 px gold top rule. Pair with EndPanel().
    bool BeginPanel(const char* id, ImVec2 pos, ImVec2 size, ImGuiWindowFlags extra = 0);
    void EndPanel();
    void Heading(const char* text);
    void Tooltip(const char* line1, const char* line2 = nullptr);

    struct Prompt {
        std::uint32_t padButton;   // B21UI_PAD_*
        const char* keyLabel;      // e.g. "Esc", "Enter", "E"
        const char* label;         // e.g. "Back"
    };
    // Bottom-right prompt row; draws pad glyphs or key caps depending on the active device.
    void PromptBar(std::span<const Prompt> prompts, std::uint32_t activeDevice, ImVec2 anchorBottomRight);
}
