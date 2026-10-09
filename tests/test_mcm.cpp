#include <doctest/doctest.h>
#include "core/Mcm.h"

#include <Windows.h>
#include <fstream>

using namespace b21ui::mcm;
namespace {
    class Fake final : public Backend {
    public:
        Json values = Json::object(), calls = Json::array(), events = Json::array();
        bool writable = true;
        bool Installed(std::string_view plugin) override { return plugin == "Fallout4.esm"; }
        std::optional<Json> Read(const Json&, std::string_view, std::string_view id) override {
            return values.contains(std::string(id)) ? std::optional<Json>(values[std::string(id)]) : std::nullopt;
        }
        bool Write(const Json&, std::string_view, std::string_view id, const Json& value) override {
            if (!writable) return false;
            values[std::string(id)] = value;
            return true;
        }
        bool Call(const Json& action, const Json& args) override { calls.push_back({action, args}); return true; }
        void Event(std::string_view name, const Json& args) override { events.push_back({std::string(name), args}); }
    };
    struct Fixture {
        std::filesystem::path root = std::filesystem::absolute("build/mcm-tests-" + std::to_string(::GetCurrentProcessId()));
        Fixture() { std::filesystem::create_directories(root / "Config/Demo"); }
        ~Fixture() { std::error_code error; std::filesystem::remove_all(root, error); }
        void Write(const char* path, const std::string& text) {
            const auto file = root / path;
            std::filesystem::create_directories(file.parent_path());
            std::ofstream(file, std::ios::binary) << text;
        }
    };
    Json Config() {
        return Json::parse(R"({"modName":"Demo","displayName":"Demo settings","content":[
          {"type":"switcher","id":"bEnabled:Main","text":"Enable","groupControl":1,"valueOptions":{"sourceType":"ModSettingBool"}},
          {"type":"slider","id":"iCount:Main","text":"Count","groupCondition":1,"valueOptions":{"sourceType":"ModSettingInt","min":0,"max":10,"step":2},
           "action":{"type":"CallGlobalFunction","script":"Demo","function":"Apply","params":["{value}","{b}{value}","value={value}"]}},
          {"type":"button","text":"Reset","action":{"type":"CallFunction","form":"Demo.esp|800","function":"Reset","params":["{i}2"]}}
        ],"pages":[{"pageDisplayName":"Extra","content":[{"type":"text","text":"<b>Native</b><br/>settings"}]}]})");
    }
}

TEST_CASE("MCM replacement is hidden for an installed or loaded original DLL") {
    Fixture fixture;
    CHECK(ReplacementEnabled(fixture.root, false));
    CHECK_FALSE(ReplacementEnabled(fixture.root, true));
    fixture.Write("MCM.dll", "installed");
    CHECK_FALSE(ReplacementEnabled(fixture.root, false));
}
TEST_CASE("MCM native controls read values, filter groups and route typed callbacks") {
    Fake backend;
    backend.values = {{"bEnabled:Main", false}, {"iCount:Main", 4}};
    auto menu = Parse(Config(), backend);
    REQUIRE(menu.pages.size() == 2);
    auto& page = menu.pages[0];
    CHECK_FALSE(page.rows[1].visible);
    CHECK_FALSE(Change(page, 1, 8, backend));
    CHECK(Change(page, 0, true, backend));
    CHECK(page.rows[1].visible);
    CHECK(Change(page, 1, 11, backend));
    CHECK(backend.values["iCount:Main"] == 10);
    CHECK(backend.calls[0][1] == Json::array({10, true, "value=10"}));
    CHECK(backend.events[2][0] == "OnMCMSettingChange");
    CHECK(backend.events[3] == Json::array({"OnMCMSettingChange|Demo", Json::array({"Demo", "iCount:Main"})}));
    CHECK(Change(page, 2, nullptr, backend));
    CHECK(backend.calls[1][1] == Json::array({2}));
    CHECK(menu.pages[1].rows[0].text == "Native\nsettings");
    page.rows[1].definition["type"] = "textinputInt";
    CHECK(Change(page, 1, -5, backend));
    CHECK(backend.values["iCount:Main"] == 0);
    CHECK(Change(page, 1, 7, backend));
    CHECK(backend.values["iCount:Main"] == 8);
}
TEST_CASE("MCM failed writes never dispatch callbacks or setting events") {
    Fake backend;
    backend.values = {{"bEnabled:Main", true}, {"iCount:Main", 4}};
    auto menu = Parse(Config(), backend);
    backend.writable = false;
    CHECK_FALSE(Change(menu.pages[0], 1, 8, backend));
    CHECK(backend.values["iCount:Main"] == 4);
    CHECK(backend.events.empty());
    CHECK(backend.calls.empty());
    CHECK_FALSE(menu.pages[0].rows[1].status.empty());
    Refresh(menu.pages[0], backend);
    CHECK_FALSE(menu.pages[0].rows[1].status.empty());
    backend.writable = true;
    CHECK(Change(menu.pages[0], 1, 8, backend));
    CHECK(menu.pages[0].rows[1].status.empty());
}
TEST_CASE("MCM requirements hide optional pages and expose missing required plugins") {
    Fake backend;
    auto config = Config();
    config["pages"].push_back({{"pageDisplayName", "Missing"}, {"pluginRequirements", {"Absent.esp"}}, {"hideIfMissingReqs", true}});
    CHECK(Parse(config, backend).pages.size() == 2);
    config["pluginRequirements"] = {"Absent.esp"};
    auto menu = Parse(config, backend);
    REQUIRE(menu.pages.size() == 1);
    CHECK(menu.pages[0].error.find("Absent.esp") != std::string::npos);
    config["hideIfMissingReqs"] = true;
    CHECK(Parse(config, backend).pages.empty());
}
TEST_CASE("MCM AND OR ONLY conditions match the original default group semantics") {
    CHECK(Visible(nullptr, {0}));
    CHECK(Visible(0, {0}));
    CHECK(Visible(Json::array({1, 2}), {0, 2}));
    CHECK_FALSE(Visible(Json{{"AND", {1, 2}}}, {0, 2}));
    CHECK(Visible(Json{{"ONLY", {1, 2}}}, {0, 1, 2}));
    CHECK_FALSE(Visible(Json{{"ONLY", {1}}}, {0, 1, 2}));
}
TEST_CASE("MCM Flash controls and Flash extension callbacks are explicitly unavailable") {
    Fake backend;
    const auto config = Json::parse(R"({"modName":"Demo","content":[
      {"type":"customClipLoader","libName":"Panel"},
      {"type":"button","text":"External","action":{"type":"CallExternalFunction","plugin":"Demo","function":"Apply"}}
    ]})");
    auto menu = Parse(config, backend);
    CHECK_FALSE(menu.pages[0].rows[0].error.empty());
    CHECK_FALSE(Change(menu.pages[0], 1, nullptr, backend));
    CHECK(backend.calls.empty());
}
TEST_CASE("MCM sessions preserve open state while switching mods and close once") {
    Fake backend;
    Session session;
    session.Select("A", backend);
    session.Select("B", backend);
    session.PageChanged(backend);
    session.Close(backend);
    session.Close(backend);
    const Json expected = {"OnMCMOpen", "OnMCMMenuOpen", "OnMCMMenuOpen|A", "OnMCMMenuClose|A",
        "OnMCMMenuOpen", "OnMCMMenuOpen|B", "OnMCMMenuOpen", "OnMCMMenuClose|B", "OnMCMMenuClose", "OnMCMClose"};
    Json names = Json::array();
    for (const auto& event : backend.events) { names.push_back(event[0]); CHECK(event[1].empty()); }
    CHECK(names == expected);
}
TEST_CASE("MCM INI defaults and saved overrides round trip all supported types") {
    Fixture fixture;
    fixture.Write("Config/Demo/settings.ini", "[Main]\nbEnabled=1\niCount=2\nfSpeed=1.25\nsName=First\n[Other]\niCount=99\n");
    fixture.Write("Settings/Demo.ini", "[Main]\niCount=6\n");
    SettingsStore store;
    store.Load(fixture.root);
    CHECK(store.Get("Demo", "bEnabled:Main") == true);
    CHECK(store.Get("Demo", "iCount:Main") == 6);
    CHECK(store.Get("Demo", "fSpeed:Main").get<float>() == doctest::Approx(1.25F));
    CHECK(store.Set("Demo", "sName:Main", "New value"));
    CHECK(store.Set("Demo", "bEnabled:Main", false));
    CHECK_FALSE(store.Set("../Other", "iCount:Main", 5));
    CHECK_FALSE(store.Set("Demo", "missing:Main", 5));
    SettingsStore loaded;
    loaded.Load(fixture.root);
    CHECK(loaded.Get("Demo", "sName:Main") == "New value");
    CHECK(loaded.Get("Demo", "iCount:Other") == 99);
    CHECK(loaded.Get("Demo", "bEnabled:Main") == false);
    std::ifstream saved(fixture.root / "Settings/Demo.ini");
    const std::string text((std::istreambuf_iterator<char>(saved)), {});
    CHECK(text.find("iCount=99") == std::string::npos);
}
TEST_CASE("MCM catalog merges extension pages and keeps malformed mods visible") {
    Fixture fixture;
    fixture.Write("Config/Demo/config.json", Config().dump());
    fixture.Write("Config/Extension/config.json", R"({"modName":"Extension","ownerModName":"Demo","pages":[{"pageDisplayName":"Extended","content":[{"type":"text","text":"Extension"}]}]})");
    fixture.Write("Config/Broken/config.json", "{broken");
    Fake backend;
    auto menus = Load(fixture.root / "Config", backend);
    REQUIRE(menus.size() == 2);
    CHECK_FALSE(menus[0].pages[0].error.empty());
    REQUIRE(menus[1].pages.size() == 3);
    CHECK(menus[1].pages[2].mod == "Extension");
}
TEST_CASE("MCM keybinds import, detect conflicts, clear, persist and rematch") {
    Fixture fixture;
    fixture.Write("Config/Demo/keybinds.json", R"({"modName":"Demo","keybinds":[
      {"id":"one","desc":"One","action":{"type":"CallGlobalFunction","script":"Demo","function":"One"}},
      {"id":"two","desc":"Two","action":{"type":"SendEvent","form":"Demo.esp|800"}}]})");
    fixture.Write("Settings/Keybinds.json", R"({"version":1,"keybinds":[{"modName":"Demo","id":"one","keycode":30,"modifiers":2}]})");
    Keybinds keys;
    keys.Load(fixture.root);
    REQUIRE(keys.Match(30, 2).has_value());
    CHECK(keys.Match(30, 2)->id == "one");
    CHECK_FALSE(keys.Set("Demo", "two", 30, 2));
    CHECK(keys.Set("Demo", "one", 0, 0));
    CHECK(keys.Set("Demo", "two", 30, 2));
    Keybinds loaded;
    loaded.Load(fixture.root);
    REQUIRE(loaded.Match(30, 2).has_value());
    CHECK(loaded.Match(30, 2)->id == "two");
    CHECK(loaded.Find("Demo", "one")->key == 0);
    CHECK_FALSE(loaded.Match(0, 0).has_value());
    REQUIRE(loaded.Handle(30, 2, true).has_value());
    REQUIRE(loaded.Handle(30, 0, false).has_value());
    CHECK_FALSE(loaded.Handle(30, 0, false).has_value());
}
