#pragma once
// Look-neutral core shared by every B21UI style (fo76, modern, fo4): per-context state, fonts,
// controller helpers, the game cursor and image modes. A style header includes this; menus include a
// style header.
#include "b21ui/Abi.h"
#include "b21ui/CssFilter.h"
#include "b21ui/Texture.h"

#include <imgui.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <unordered_map>

namespace b21ui::common {
    // Per-ImGui-context storage: ImFont pointers belong to one context's atlas, and one DLL may own
    // several clients. One slot per type.
    template <class T>
    T& ContextSlot() {
        static std::unordered_map<ImGuiContext*, T> byContext;
        return byContext[ImGui::GetCurrentContext()];
    }

    struct Fonts {
        ImFont* regular{};
        ImFont* bold{};
    };

    // Sets the io flags every B21UI view needs (no ini, keyboard + gamepad nav) and loads Roboto
    // Condensed from `fontDir` with Segoe UI Symbol merged in (falls back to ImGui's default font).
    // The client runtime calls it once per context; styles reuse these fonts.
    const Fonts& InitBaseContext(const std::filesystem::path& fontDir);
    const Fonts& CurrentFonts();
    struct FontMerge {
        std::filesystem::path file;
        ImVec2 glyphOffset{};
    };
    // One TTF/OTF plus glyph fonts merged into it (missing merge files are skipped). Null when `file`
    // is missing.
    ImFont* LoadFont(const std::filesystem::path& file, std::span<const FontMerge> merge = {});
    // <Windows>/Fonts/<name>, e.g. seguisym.ttf (Segoe UI Symbol: arrows, stars and box glyphs).
    std::filesystem::path WindowsFont(const wchar_t* name);

    // The style a context uses registers how to rebuild itself from ImGui defaults at a UI scale; a
    // scale change rebuilds only that style. The last registration wins.
    void SetStyleRebuild(std::function<void(float uiScale)> rebuild);
    void RebuildStyle();

    // Resolution scale (display height / 1080, never below 1); the client runtime sets it each frame.
    void SetUiScale(float scale);
    float UiScale();
    // Device the host last saw input from (B21UI_DEVICE_*); the client runtime sets it each frame.
    std::uint32_t ActiveDevice();
    void SetActiveDevice(std::uint32_t device);
    // The player's HUD color from the game (FO4 default green until the host sends one); the client
    // runtime sets it each frame. The cursor, the on-screen keyboard and the fo4 style use it.
    ImU32 HudColor();
    void SetHudColor(ImU32 color);

    // The last item has keyboard/controller focus and the nav cursor is showing.
    bool ItemNavFocused();
    // For a focused (not active) slider-like item: D-pad / left stick left-right move `value` by
    // `step` of the range without pressing A first. Call right after the item.
    bool NavAdjust(float* value, float min, float max, float step);

    inline constexpr std::uint32_t kNoPad = 0xFFFFFFFFu;
    // OR into a Button's padButton to show the glyph without pressing the button: for buttons whose
    // pad button the view already handles globally.
    inline constexpr std::uint32_t kPadGlyphOnly = 0x80000000u;
    namespace pad {
        inline constexpr ImU32 A = IM_COL32(0x92, 0xD4, 0x77, 0xFF);
        inline constexpr ImU32 B = IM_COL32(0xFF, 0x89, 0x7E, 0xFF);
        inline constexpr ImU32 X = IM_COL32(0x8A, 0xCA, 0xFF, 0xFF);
        inline constexpr ImU32 Y = IM_COL32(0xF5, 0xCB, 0x5B, 0xFF);
    }
    // Controller button glyph: a coloured circle for A/B/X/Y, a pill with the button name otherwise.
    // `ink` colours the pill's text and outline.
    void PadGlyph(ImDrawList* list, ImVec2 center, float radius, std::uint32_t padButton, ImU32 ink = IM_COL32(0xFF, 0xFF, 0xCB, 0xFF));
    float PadGlyphWidth(float radius, std::uint32_t padButton);
    // Pressed this frame (no repeat) while the current window tree is focused.
    bool PadPressed(std::uint32_t padButton);

    // FO4-style arrowhead pointer, tip at `tip`.
    void DrawGameCursor(ImDrawList* list, ImVec2 tip, ImU32 color, float scale);
    // A whole texture with the Pip-Boy screen look: dark grayscale, screen-tinted toward `shadow` and
    // `dim` toward the image edges.
    void DrawPipBoyImage(ImDrawList* list, const Texture& texture, ImVec2 min, ImVec2 max, ImU32 shadow, ImU32 dim,
                         float alpha = 1.0F);
    // A whole texture through a CSS filter chain (b21ui/CssFilter.h), up to 8 steps.
    void DrawFilteredImage(ImDrawList* list, const Texture& texture, ImVec2 min, ImVec2 max,
                           std::span<const css::ColorMatrix> steps, float alpha = 1.0F);
    void DrawColorizedImage(ImDrawList* list, const Texture& texture, ImVec2 min, ImVec2 max, float alpha = 1.0F);
    struct CrtOptions {
        bool screen = false;    // scanlines, aperture grille, slight colour fringing, glow, grain, rare line slips
        bool curved = false;    // bent glass
        bool flicker = false;   // gentle phosphor shimmer and a faint rolling refresh band
    };
    // An old CRT over everything already drawn in [min, max]; each option works alone. It copies the
    // render target, so call it last in the frame (anything drawn after it, such as the cursor, stays crisp).
    void DrawCrtEffect(ImDrawList* list, ImVec2 min, ImVec2 max, const CrtOptions& options);
    // Drops the shared D3D objects; the client runtime calls it on device loss.
    void ReleaseDeviceObjects();

    struct PadTuningValues {
        float deadzone = 0.2F, cursorSpeed = 800.0F, panSpeed = 1000.0F, exponent = 1.0F;
    };
    // Data/F4SE/Plugins/B21UI.ini, section [Gamepad], keys fDeadzone, fCursorSpeed, fPanSpeed,
    // fExponent. Read once; missing file or keys keep the defaults.
    const PadTuningValues& PadTuning();
    // Controller pointer speed multiplier shared by every B21UI view; the map's Cursor speed option sets
    // it. B21UI.ini [Gamepad] fPointerMultiplier (default 1.5). Each DLL caches it, so views re-read it
    // when they gain focus to pick up a change made in another mod's menu.
    float PointerMultiplier();
    void RefreshPointerMultiplier();
    void SetPointerMultiplier(float multiplier);
}
