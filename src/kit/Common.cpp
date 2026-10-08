#include "b21ui/Common.h"
#include "b21ui/Paths.h"
#include "client/InputMap.h"
#include "core/PadCursor.h"

#include <imgui_internal.h>
#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <format>
#include <string>

namespace b21ui::common {
    namespace {
        struct State {
            Fonts fonts;
            float uiScale = 1.0F;
            std::uint32_t device = B21UI_DEVICE_KEYBOARD_MOUSE;
            ImU32 hud = IM_COL32(18, 255, 21, 255);
            std::function<void(float)> rebuild;
        };
        State& Current() { return ContextSlot<State>(); }

        float ReadFloat(const std::wstring& ini, const wchar_t* key, float fallback, float lo, float hi) {
            wchar_t buffer[64]{};
            ::GetPrivateProfileStringW(L"Gamepad", key, L"", buffer, 64, ini.c_str());
            if (!buffer[0]) return fallback;
            wchar_t* end{};
            const float value = std::wcstof(buffer, &end);
            return end == buffer ? fallback : std::clamp(value, lo, hi);
        }

        struct PadLabel {
            const char* text;
            ImU32 color;
        };

        PadLabel LabelOf(std::uint32_t button, ImU32 ink) {
            switch (button) {
            case B21UI_PAD_A: return {"A", pad::A};
            case B21UI_PAD_B: return {"B", pad::B};
            case B21UI_PAD_X: return {"X", pad::X};
            case B21UI_PAD_Y: return {"Y", pad::Y};
            case B21UI_PAD_LB: return {"LB", ink};
            case B21UI_PAD_RB: return {"RB", ink};
            case B21UI_PAD_LT: return {"LT", ink};
            case B21UI_PAD_RT: return {"RT", ink};
            case B21UI_PAD_BACK: return {"View", ink};
            case B21UI_PAD_START: return {"Menu", ink};
            case B21UI_PAD_LS: return {"LS", ink};
            case B21UI_PAD_RS: return {"RS", ink};
            case B21UI_PAD_DPAD_UP: return {"\xE2\x86\x91", ink};
            case B21UI_PAD_DPAD_DOWN: return {"\xE2\x86\x93", ink};
            case B21UI_PAD_DPAD_LEFT: return {"\xE2\x86\x90", ink};
            case B21UI_PAD_DPAD_RIGHT: return {"\xE2\x86\x92", ink};
            default: return {"", ink};
            }
        }
    }

    std::filesystem::path WindowsFont(const wchar_t* name) {
        wchar_t windows[MAX_PATH]{};
        if (!::GetWindowsDirectoryW(windows, MAX_PATH)) ::GetEnvironmentVariableW(L"WINDIR", windows, MAX_PATH);
        return std::filesystem::path(windows) / "Fonts" / name;
    }

    ImFont* LoadFont(const std::filesystem::path& file, std::span<const FontMerge> merge) {
        if (!std::filesystem::exists(file)) return nullptr;
        auto& io = ImGui::GetIO();
        auto* font = io.Fonts->AddFontFromFileTTF(file.string().c_str());
        if (!font) return nullptr;
        for (const auto& extra : merge) {
            if (!std::filesystem::exists(extra.file)) continue;
            ImFontConfig config;
            config.MergeMode = true;
            config.GlyphOffset = extra.glyphOffset;
            io.Fonts->AddFontFromFileTTF(extra.file.string().c_str(), 0.0F, &config);
        }
        return font;
    }

    const Fonts& InitBaseContext(const std::filesystem::path& fontDir) {
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
        // Roboto Condensed has no arrows, stars or box glyphs; Segoe UI Symbol (every Windows) fills them in.
        const std::array symbols{FontMerge{WindowsFont(L"seguisym.ttf")}};
        auto& fonts = Current().fonts;
        fonts.regular = LoadFont(fontDir / "RobotoCondensed-Regular.ttf", symbols);
        fonts.bold = LoadFont(fontDir / "RobotoCondensed-Bold.ttf", symbols);
        if (!fonts.regular) fonts.regular = io.Fonts->AddFontDefault();
        if (!fonts.bold) fonts.bold = fonts.regular;
        return fonts;
    }

    const Fonts& CurrentFonts() { return Current().fonts; }

    void SetStyleRebuild(std::function<void(float)> rebuild) { Current().rebuild = std::move(rebuild); }

    void RebuildStyle() {
        auto& current = Current();
        if (!current.rebuild) return;
        // ScaleAllSizes compounds, so every rebuild starts from ImGui's defaults.
        ImGui::GetStyle() = ImGuiStyle();
        current.rebuild(current.uiScale);
    }

    void SetUiScale(float scale) {
        auto& current = Current();
        if (scale == current.uiScale) return;
        current.uiScale = scale;
        RebuildStyle();
    }

    float UiScale() { return Current().uiScale; }
    std::uint32_t ActiveDevice() { return Current().device; }
    void SetActiveDevice(std::uint32_t device) { Current().device = device; }
    ImU32 HudColor() { return Current().hud; }
    void SetHudColor(ImU32 color) { Current().hud = color; }

    // ImGui keeps a nav focus for mouse users too (e.g. a newly focused window's first item); it only
    // shows while the nav cursor is visible.
    bool ItemNavFocused() { return ImGui::IsItemFocused() && GImGui->NavCursorVisible; }

    bool NavAdjust(float* value, float min, float max, float step) {
        if (!ImGui::IsItemFocused() || ImGui::IsItemActive()) return false;
        const ImGuiKey keys[]{ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickLeft,
                              ImGuiKey_GamepadLStickRight};
        for (const auto key : keys) ImGui::SetItemKeyOwner(key);
        float delta = 0.0F;
        for (int i = 0; i < 4; ++i)
            if (ImGui::IsKeyPressed(keys[i], ImGuiInputFlags_Repeat, ImGui::GetItemID())) delta += i % 2 ? step : -step;
        if (delta == 0.0F) return false;
        *value = std::clamp(*value + delta * (max - min), min, max);
        return true;
    }

    float PadGlyphWidth(float r, std::uint32_t button) {
        if (button <= B21UI_PAD_Y) return r * 2.0F;
        return std::max(r * 2.8F, ImGui::CalcTextSize(LabelOf(button, 0).text).x + r * 0.8F);
    }

    void PadGlyph(ImDrawList* list, ImVec2 c, float r, std::uint32_t button, ImU32 ink) {
        const auto [label, color] = LabelOf(button, ink);
        if (button <= B21UI_PAD_Y) {
            list->AddCircleFilled(c, r, IM_COL32(0, 0, 0, 180));
            list->AddCircle(c, r, color, 0, 2.0F);
        } else {
            const float half = PadGlyphWidth(r, button) * 0.5F;
            list->AddRectFilled({c.x - half, c.y - r * 0.8F}, {c.x + half, c.y + r * 0.8F}, IM_COL32(0, 0, 0, 180), r * 0.3F);
            list->AddRect({c.x - half, c.y - r * 0.8F}, {c.x + half, c.y + r * 0.8F}, color, r * 0.3F, 0, 1.5F);
        }
        const auto size = ImGui::CalcTextSize(label);
        list->AddText({c.x - size.x * 0.5F, c.y - size.y * 0.5F}, color, label);
    }

    bool PadPressed(std::uint32_t button) {
        return button != kNoPad && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
               ImGui::IsKeyPressed(client::KeyFromPad(button & ~kPadGlyphOnly), false);
    }

    namespace {
        constexpr float kMinPointerMultiplier = 0.5F, kMaxPointerMultiplier = 4.0F;
        std::atomic<float> pointerMultiplier{0.0F};

        std::wstring SharedIni() { return (PluginAssetDir().parent_path() / "B21UI.ini").wstring(); }
    }

    float PointerMultiplier() {
        if (pointerMultiplier.load() == 0.0F) RefreshPointerMultiplier();
        return pointerMultiplier.load();
    }

    void RefreshPointerMultiplier() {
        pointerMultiplier = ReadFloat(SharedIni(), L"fPointerMultiplier", core::kDefaultPointerMultiplier,
                                      kMinPointerMultiplier, kMaxPointerMultiplier);
    }

    void SetPointerMultiplier(float multiplier) {
        multiplier = std::clamp(multiplier, kMinPointerMultiplier, kMaxPointerMultiplier);
        pointerMultiplier = multiplier;
        ::WritePrivateProfileStringW(L"Gamepad", L"fPointerMultiplier", std::format(L"{:.2f}", multiplier).c_str(),
                                     SharedIni().c_str());
    }

    const PadTuningValues& PadTuning() {
        static const PadTuningValues values = [] {
            const auto ini = (PluginAssetDir().parent_path() / "B21UI.ini").wstring();
            PadTuningValues v;
            v.deadzone = ReadFloat(ini, L"fDeadzone", v.deadzone, 0.0F, 0.9F);
            v.cursorSpeed = ReadFloat(ini, L"fCursorSpeed", v.cursorSpeed, 100.0F, 5000.0F);
            v.panSpeed = ReadFloat(ini, L"fPanSpeed", v.panSpeed, 100.0F, 5000.0F);
            v.exponent = ReadFloat(ini, L"fExponent", v.exponent, 0.5F, 3.0F);
            return v;
        }();
        return values;
    }
}
