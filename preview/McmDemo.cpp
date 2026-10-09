#include "preview/McmDemo.h"
#include "b21ui/Settings.h"
#include "b21ui/B21UI.h"
#include "b21ui/modern/Widgets.h"
#include "client/McmView.h"
#include "core/Mcm.h"

#include <fstream>
#include <memory>
#include <iostream>

namespace b21ui::preview {
    namespace {
        using namespace mcm;
        class DemoBackend final : public Backend {
        public:
            std::map<std::string, Json> values;
            bool Installed(std::string_view plugin) override { return plugin == "Fallout4.esm"; }
            std::optional<Json> Read(const Json&, std::string_view mod, std::string_view id) override {
                const auto found = values.find(std::string(mod) + ":" + std::string(id));
                return found == values.end() ? std::nullopt : std::optional<Json>(found->second);
            }
            bool Write(const Json&, std::string_view mod, std::string_view id, const Json& value) override {
                values[std::string(mod) + ":" + std::string(id)] = value;
                std::cout << "MCM setting " << mod << " / " << id << " = " << value.dump() << '\n';
                return true;
            }
            bool Call(const Json& action, const Json& args) override {
                std::cout << "MCM callback " << action.dump() << " " << args.dump() << '\n'; return true;
            }
            void Event(std::string_view event, const Json& args) override { std::cout << event << " " << args.dump() << '\n'; }
        } backend;
        class DemoClient final : public Client {
        public:
            explicit DemoClient(Menu menu) : menu_(std::move(menu)) {}
            void Register() {
                b21ui::Register(*this, {.name = menu_.mod.c_str(), .settings = true, .settingsLabel = menu_.name.c_str(),
                    .settingsIcon = modern::icon::Gear, .settingsCategory = "MCM"});
            }
            void DrawSettingsNavigation(const FrameContext&) override {
                View::Navigation(menu_, page_);
            }
            void Draw(const FrameContext&) override {
                view_.Draw(menu_, page_, [&](std::size_t row, const Json& value) { Change(menu_.pages[page_], row, value, backend); });
            }
        private:
            Menu menu_;
            std::size_t page_{};
            View view_;
        };
        std::vector<std::unique_ptr<DemoClient>> clients;
    }
    void RegisterMcmDemo(int argc, char** argv) {
        std::filesystem::path configs = "preview/fixtures/mcm/Config";
        for (int i = 1; i + 1 < argc; ++i) if (std::string_view(argv[i]) == "--mcm-configs") configs = argv[i + 1];
        backend.values = {{"SurvivalDemo:bEnabled:Main", true}, {"SurvivalDemo:iDamage:Main", 100},
            {"SurvivalDemo:iDifficulty:Main", 1}, {"SurvivalDemo:sProfile:Main", "My survival profile"},
            {"SurvivalDemo:QuickSave", Json::array({63, 0})}, {"CompanionDemo:bEssential:Main", true},
            {"CompanionDemo:fDistance:Main", 150.0}};
        for (auto& menu : mcm::Load(configs, backend)) {
            auto client = std::make_unique<DemoClient>(std::move(menu));
            client->Register();
            clients.push_back(std::move(client));
        }
    }
}
