#pragma once

#include "b21ui/B21UI.h"
#include "b21ui/Windows.h"
#include "b21ui/modern/Peek.h"
#include "b21ui/modern/Theme.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace b21ui::modern {
    struct TableState {
        std::string sortKey = "name";
        bool ascending = true;
        int page = 0;
        std::uint64_t pendingRequest = 0;
        std::string selected;  // formID / row key
    };
}

namespace b21ui::modern::w {
    // JSON readers that never throw: missing keys give the fallback, numbers print without noise.
    const nlohmann::json& Obj(const nlohmann::json& value, std::string_view key);
    const nlohmann::json& Arr(const nlohmann::json& value, std::string_view key);
    std::string Str(const nlohmann::json& value, std::string_view key, std::string_view fallback = "");
    std::string Text(const nlohmann::json& value);  // any JSON scalar as display text
    double Num(const nlohmann::json& value, std::string_view key, double fallback = 0.0);
    bool Bool(const nlohmann::json& value, std::string_view key, bool fallback = false);
    std::string First(std::initializer_list<std::string> values, std::string_view fallback = "");
    std::string Number(double value, int decimals = 3);
    std::string Hex(std::string_view formID);  // "0x" + formID, "—" when empty
    bool Matches(const nlohmann::json& value, std::string_view query);  // case-insensitive, whole JSON dump

    struct FontScope {
        FontScope(ImFont* font, float cssSize);
        ~FontScope();
        FontScope(const FontScope&) = delete;
        FontScope& operator=(const FontScope&) = delete;
    };

    enum class Tone { Neutral, Accent, Ok, Warn, Error };
    ImU32 ToneColor(Tone tone);

    void Title(std::string_view text);
    void Heading(std::string_view text, int count = -1);
    void Muted(std::string_view text);
    void Wrapped(std::string_view text, ImU32 color = theme::Text);
    void Mono(std::string_view text, ImU32 color = theme::Text);
    void Badge(std::string_view text, Tone tone = Tone::Neutral);
    void Callout(std::string_view text, Tone tone = Tone::Warn);
    void EmptyState(std::string_view text);

    bool Button(const char* label, bool enabled = true, ImVec2 size = {0, 0});
    bool Primary(const char* label, bool enabled = true, ImVec2 size = {0, 0});
    bool Danger(const char* label, bool enabled = true, ImVec2 size = {0, 0});
    // Borderless square icon button; `active` tints it with the accent.
    bool Icon(const char* id, const char* glyph, const char* tooltip = nullptr, bool active = false, bool enabled = true);
    bool Switch(const char* label, bool on, bool enabled = true);
    // A row of joined toggle buttons; returns the index clicked or -1.
    int Segmented(const char* id, std::span<const char* const> labels, int current);
    bool Search(const char* id, char* buffer, std::size_t size, const char* hint, float width);
    // "‹ 3 / 12 ›"; returns -1, 0 or +1.
    int Pager(const char* id, int page, int pageCount);
    void Tooltip(const char* text);
    // Background opacity and text size sliders for the shared theme::Appearance; every modern app follows
    // the saved result. Returns true while a value changed this frame.
    bool AppearanceSettings();
    // The dimmed backdrop and a centred app window (the whole screen when `fullscreen`), with zero padding and
    // item spacing for the app's own layout. maxSize 0 = no cap. Pair with ImGui::End(); `size` gets the window size.
    bool BeginAppWindow(const char* id, ImVec2 display, bool fullscreen, ImVec2 maxSize, ImVec2& size);
    // Expand / compress icon, plus Ctrl+Enter. Flips `fullscreen` and returns true when toggled.
    bool FullscreenToggle(bool& fullscreen);

    // ---- The title-bar kit: a user-sized window, the quick-settings cog, the peek eye, the burger.

    // Like BeginAppWindow, but the player can move and resize it. Its rectangle is kept per
    // `saveName` in B21UI.ini and restored (clamped to `minSize` and the display) next time.
    // Always pair with ImGui::End().
    bool BeginResizableAppWindow(const char* id, std::string_view saveName, ImVec2 display, ImVec2 minSize,
                                 ImVec2 defaultSize, ImVec2& size);

    // The cog and its popover: the client's own rows (if any), then Appearance. Set
    // `toggleRequested` from a hotkey to open or close it.
    struct QuickSettingsState {
        bool open = false;
        bool toggleRequested = false;
        bool openAtClick = false;
    };
    bool QuickSettings(const char* id, QuickSettingsState& state, const std::function<void()>& rows = {},
                       const char* tooltip = "Quick settings");

    // The eye: click to toggle the peek, press and hold to peek while held. Returns true while peeking.
    bool PeekButton(const char* id, Peek& peek);
    // Scales the alpha of everything the named window and its child windows drew this frame, and of the
    // background scrim. Call after ImGui::End() of that window, with Peek::Opacity().
    void FadeWindow(const char* windowName, float opacity);

    // The burger and the page list of a sidebar. `windowWidth` is the app window's width: below
    // layout::kCompactSidebarBelow the sidebar shows icons whatever the burger says, without
    // changing the saved choice.
    struct NavItem {
        const char* id;
        const char* label;
        const char* icon;
        const char* group = nullptr;
        std::string count{};
    };
    bool SidebarCompact(std::string_view saveName, float windowWidth);
    float SidebarWidth(bool compact);
    // Returns the clicked item's index, or -1.
    int Sidebar(std::string_view saveName, std::span<const NavItem> items, int active, float windowWidth);
    // Top-bar icon listing the other installed B21 windows (b21ui/Windows.h); picking one closes `self` and
    // opens it. Draws nothing when no other window is installed; returns true after a switch.
    bool WindowLauncher(Client& self, std::string_view selfId);

    // Two-column label/value grid.
    bool BeginProperties(const char* id);
    void Property(std::string_view label, std::string_view value, bool mono = false, ImU32 color = theme::Text);
    void EndProperties();

    // Sortable catalog table over an array of JSON rows.
    struct Column {
        const char* label;
        const char* key;       // row field shown and sorted by
        float width = 0.0F;    // 0 = stretch
        bool mono = false;
        bool sortable = true;
        std::function<std::string(const nlohmann::json&)> text{};  // overrides row[key]
    };
    struct TableEvents {
        int clicked = -1;
        int activated = -1;  // double-click
        bool sortChanged = false;
    };
    // `rowKey` names the field identifying a row for TableState::selected. `trailing` (optional)
    // draws extra per-row controls in a last fixed-width column.
    TableEvents Table(const char* id, std::span<const Column> columns, const nlohmann::json& rows, TableState& table,
                      const char* rowKey = "formID", float trailingWidth = 0.0F,
                      const std::function<void(const nlohmann::json&, int)>& trailing = {}, float height = 0.0F);
}
