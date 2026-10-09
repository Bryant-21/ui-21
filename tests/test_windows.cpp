#include <doctest/doctest.h>
#include "b21ui/Windows.h"

#include <ostream>
#include <set>
#include <string>
#include <string_view>

TEST_CASE("every catalog window has a unique id and all its fields") {
    std::set<std::string_view> ids;
    for (const auto& window : b21ui::Windows()) {
        CAPTURE(std::string(window.id));
        CHECK(ids.insert(window.id).second);
        CHECK_FALSE(window.label.empty());
        CHECK(std::string_view(window.glyph).size() == 3);
        if (window.id == "ui21Settings") {
            CHECK(window.label == "UI 21 Settings");
            CHECK(window.module == nullptr);
            CHECK(window.plugin == nullptr);
        } else {
            CHECK(std::wstring_view(window.module).ends_with(L".dll"));
            CHECK_FALSE(std::string_view(window.plugin).empty());
        }
    }
}

TEST_CASE("installed windows are the loaded ones, without the caller") {
    const auto loaded = [](const b21ui::Window& window) { return window.id != "musicPlayer"; };
    std::set<std::string_view> ids;
    for (const auto* window : b21ui::InstalledWindows("ui21Settings", loaded)) ids.insert(window->id);
    CHECK(ids == std::set<std::string_view>{"devTools", "autoConflictResolver", "fullScreenMap"});
    CHECK(b21ui::InstalledWindows("devTools", [](const b21ui::Window&) { return false; }).empty());
}
