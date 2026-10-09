#include <doctest/doctest.h>
#include "core/SettingsRegistry.h"

#include <string_view>
#include <ostream>

TEST_CASE("settings registry includes only flagged modal panels") {
    b21ui::core::SettingsRegistry registry;
    B21UI_ClientDesc desc{};
    desc.size = sizeof(desc);
    desc.name = "settings-client";
    desc.settingsLabel = "Dev Tools";
    desc.settingsIcon = "\xEF\x97\xBD";
    registry.Add(1, desc);
    CHECK(registry.Resolve() == 0);
    desc.flags = B21UI_FLAG_SETTINGS;
    desc.kind = B21UI_KIND_OVERLAY;
    registry.Add(2, desc);
    CHECK(registry.Resolve() == 0);
    desc.kind = B21UI_KIND_MODAL;
    registry.Add(3, desc);
    registry.Add(3, desc);
    B21UI_SettingsPanel panel{};
    CHECK(registry.Panels(&panel, 1) == 1);
    CHECK(panel.id == 3);
    CHECK(std::string_view(panel.label) == "Dev Tools");
    CHECK(std::string_view(panel.icon) == "\xEF\x97\xBD");
    CHECK(std::string_view(registry.Category(3)) == "UI 21");
    desc.settingsCategory = "MCM";
    registry.Add(4, desc);
    CHECK(std::string_view(registry.Category(4)) == "MCM");
}

TEST_CASE("settings selection remembers a valid panel and rejects ordinary clients") {
    b21ui::core::SettingsRegistry registry;
    B21UI_ClientDesc desc{};
    desc.size = sizeof(desc);
    desc.flags = B21UI_FLAG_SETTINGS;
    desc.name = "Tales from Appalachia";
    registry.Add(4, desc);
    desc.name = "Dev Tools";
    registry.Add(8, desc);
    CHECK(registry.Resolve() == 4);
    CHECK(registry.Resolve(9) == 0);
    registry.Select(8);
    registry.Select(9);
    CHECK(registry.Resolve() == 8);
    CHECK(registry.Resolve(4) == 4);
    B21UI_SettingsPanel panels[2]{};
    CHECK(registry.Panels(nullptr, 0) == 2);
    CHECK(registry.Panels(panels, 1) == 2);
    CHECK(panels[0].id == 4);
    CHECK(panels[1].id == 0);
}

TEST_CASE("settings labels respect the legacy descriptor size and remain stable") {
    b21ui::core::SettingsRegistry registry;
    B21UI_ClientDesc desc{};
    desc.size = B21UI_CLIENTDESC_MIN_SIZE;
    desc.flags = B21UI_FLAG_SETTINGS;
    desc.name = "legacy label";
    desc.settingsLabel = "outside the declared size";
    registry.Add(1, desc);
    B21UI_SettingsPanel panel{};
    registry.Panels(&panel, 1);
    CHECK(std::string_view(panel.label) == "legacy label");
    CHECK(std::string_view(panel.icon) == "\xEF\x80\x93");
    for (std::uint32_t id = 2; id < 100; ++id) registry.Add(id, desc);
    CHECK(std::string_view(panel.label) == "legacy label");
}
