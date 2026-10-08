#pragma once
// The Fallout 4 Scaleform look (container / barter, pause / settings, message box), measured from the
// game's own SWFs: docs/fo4-style-metrics.md. Everything FO4 authors white is drawn in the player's HUD
// colour; selections are a solid HUD bar with black text. Coordinates are FO4 stage units (1280 x 720,
// scaled by display height / 720 and centred), so menus use the SWF numbers directly.
//
// fo4 menus drive their own navigation, like FO4's: widgets read the keyboard, D-pad, sticks and mouse
// themselves (fo4::InitContext turns ImGui's nav off for the context). Draw them inside Begin/EndMenu.
#include "b21ui/Common.h"
#include "core/ListModel.h"

#include <span>
#include <string>
#include <string_view>

namespace b21ui::fo4 {
    using core::ListModel;

    // Makes fo4 this context's style (fonts come from common::InitBaseContext, which the client runtime
    // already ran). Call from the client's OnContextCreated.
    void InitContext();

    // Stage units -> pixels (lengths) and stage point -> screen point.
    float Px(float stageUnits);
    ImVec2 Stage(float x, float y);

    // A full-screen, input-catching layer the menu draws into (no background). `dim` darkens the game.
    void BeginMenu(const char* id, float dim = 0.0F);
    void EndMenu();
    ImDrawList* Canvas();

    // Palette: FO4 art is white, tinted by the engine with the HUD colour.
    ImU32 Fg(float alpha = 1.0F);
    inline constexpr ImU32 Black = IM_COL32(0, 0, 0, 255);

    // Text whose glyph box starts at stage (x, y), with FO4 letter spacing (stage units).
    enum class Align { Left, Center, Right };
    void Text(float x, float y, float size, std::string_view text, ImU32 color, bool bold = false, float spacing = 0.0F,
              Align align = Align::Left);
    // Width in stage units.
    float TextWidth(float size, std::string_view text, bool bold = false, float spacing = 0.0F);

    // Menu input, read once per frame by the widgets.
    struct Nav {
        int dy = 0;         // up / down (arrows, W/S, D-pad, left stick), with repeat
        int dx = 0;         // left / right (arrows, A/D, D-pad, left stick), with repeat
        bool accept = false;  // Enter, E, A button
        bool cancel = false;  // Esc, Tab, B button
    };
    Nav ReadNav();

    // FO4 frame: horizontal bars with 4-unit ticks (top ticks down, bottom ticks up), no sides. The top
    // bar can leave a gap (for a list header) between gapFrom and gapTo.
    void Brackets(float x0, float x1, float topY, float bottomY, float gapFrom = 0.0F, float gapTo = 0.0F);
    // List header "◀ TITLE ▶" at its stage origin; returns its width (the top bar gap).
    float ListHeader(float x, float y, std::string_view title, bool arrows = true);
    // Double chevron scroll marker, `up` or down, left edge at x, top at y.
    void ScrollMarker(float x, float y, bool up);

    struct Item {
        std::string name;
        int count = 1;
        bool equipped = false;
        bool favorite = false;
        bool legendary = false;
        bool disabled = false;
    };

    struct ListResult {
        bool selectionChanged = false;
        int activated = -1;  // accept on the selected row, or a click on a row
    };

    // Inventory panels of the container / barter menus, at FO4's measured positions.
    enum class Side { Player, Other };
    // Frame, header, rows, selection bar (pointing toward the screen centre) and scroll markers. Only
    // the `focused` panel shows its selection and takes the keyboard / pad; the mouse works on both
    // (hover selects, click activates; the caller moves focus to the panel that reports it).
    ListResult InventoryPanel(Side side, std::string_view title, std::span<const Item> items, ListModel& model, bool focused,
                              const Nav& nav);

    struct Stat {
        std::string label;
        std::string value;
        bool strong = false;  // damage / ammo rows use the brighter 40 % fill
    };
    // Item card: stat rows right-aligned at stage x 722, stacking upward to y 484 (container menu).
    void ItemCard(std::span<const Stat> rows);
    // Player footer boxes: carry weight and caps.
    void InventoryFooter(std::string_view weight, std::string_view caps);

    // Pause main list: bracket frame and plain selection rects. Returns the activated index or -1.
    int PauseList(std::span<const char* const> options, ListModel& model, bool focused, const Nav& nav);

    // Settings panel frame (to the right of the pause list).
    void SettingsFrame();
    // One settings row at `row` (0-based). The selected row takes left / right.
    bool SettingSlider(int row, std::string_view label, float& value, bool selected, const Nav& nav, float step = 0.05F);
    bool SettingToggle(int row, std::string_view label, bool& on, bool selected, const Nav& nav);
    bool SettingStepper(int row, std::string_view label, int& index, std::span<const char* const> values, bool selected,
                        const Nav& nav);
    // Mouse over row `row`: settings lists select on hover like FO4.
    bool SettingRowHovered(int row);
    inline constexpr int kSettingRows = 8;

    // FO4 confirmation box centred on screen: body text and a vertical button list. Returns the chosen
    // button, -1 while open; cancel picks `cancelIndex`.
    int MessageBox(std::string_view body, std::span<const char* const> buttons, int& selected, int cancelIndex, const Nav& nav);

    struct Hint {
        std::uint32_t padButton;  // B21UI_PAD_*
        const char* key;          // keyboard key, e.g. "E", "Tab"
        const char* label;        // e.g. "Take"; drawn upper case
        bool enabled = true;
    };
    // FO4 button-hint bar centred on stage x 640 at stage y `y` (container 655, barter 688, pause 650).
    // Returns the index of a hint the mouse clicked, or -1.
    int HintBar(std::span<const Hint> hints, float y);
}
