#include <F4SE/F4SE.h>
#include <RE/Fallout.h>
#include "game/KeybindingsRuntime.h"
#include "game/McmRuntime.h"
#include "b21ui/B21UI.h"
#include "b21ui/Tasks.h"
#include "b21ui/modern/Widgets.h"
#include "client/KeybindingsView.h"
#include "core/Mcm.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>

namespace b21ui::game::KeybindingsRuntime {
    namespace {
        keys::Providers providers;
        const char* contexts[]{"Gameplay", "Menu navigation", "Menu thumbstick", "Virtual controller", "Menu cursor",
            "Menu left cursor", "Console", "Debug text", "Books", "Debug overlay", "Free camera", "Debug map",
            "Lockpicking", "V.A.T.S.", "V.A.T.S. playback", "Multiple activation", "Workshop", "Scope", "Sit / Wait",
            "Looks menu", "Workshop addendum", "Pause menu", "Level up", "Level up previous / next", "Main menu",
            "Quick container", "Special activation", "Two-button rollover", "Quick container perk", "Vertibird",
            "Videos", "Robot workbench", "Creations"};
        int Macro(int device, int raw) {
            if (raw < 0 || raw == 255) return 0;
            if (device == 0) return raw > 0 && raw < 255 ? raw : 0;
            if (device == 1) return raw < 10 ? 256 + raw : 0;
            return keys::FromGamepadMask(static_cast<unsigned int>(raw));
        }
        std::vector<keys::Binding> GameBindings() {
            std::vector<keys::Binding> result;
            const auto* control=RE::ControlMap::GetSingleton();
            if (!control) return result;
            struct HoldSetting { const char* event; const char* name; std::string value; };
            HoldSetting holds[]{
                {"ReadyWeapon","fReloadReadyDelay:Controls"}, {"Melee","fThrowDelay:Controls"},
                {"Pipboy","fPipboyLightDelay:Controls"}, {"TogglePOV","fTogglePOVDelay:Controls"},
                {"TogglePOV","fEnterWorkshopDelay:Controls"}, {"Activate","fZKeyDelay:Controls"},
                {"Activate","fPowerArmorExitDelay:Controls"}, {"Jump","fAlternateFurnitureExitDelay:Controls"}
            };
            for (auto& hold : holds) if (const auto* setting=RE::GetINISetting(hold.name);
                setting && setting->GetType()==RE::Setting::SETTING_TYPE::kFloat && std::isfinite(setting->GetFloat()) && setting->GetFloat()>=0)
                hold.value=std::format(" {} = {:.3g}s.",hold.name,setting->GetFloat());
            static_assert(std::size(contexts)==std::to_underlying(RE::UserEvents::INPUT_CONTEXT_ID::kTotal));
            for (std::size_t context=0; context<std::size(contexts); ++context) {
                const auto* table=control->controlMaps[context]; if (!table) continue;
                for (int device=0; device<3; ++device) for (const auto& mapping : table->deviceMappings[device]) {
                    const auto raw=mapping.inputKey;
                    if (raw<0 || raw==255) continue;
                    const int key=Macro(device,raw);
                    if (!key) continue;
                    keys::Binding binding{mapping.eventID.c_str(),mapping.eventID.c_str(),"Fallout 4",key};
                    binding.contexts={contexts[context]};
                    keys::DescribeGameActivation(binding,contexts[context]);
                    if (context==0) for (const auto& hold : holds) if (binding.id==hold.event) binding.conditions+=hold.value;
                    if (context>=1 && context<=5) {
                        binding.contexts.push_back("Pip-Boy");
                        binding.conditions+=" Shared menu controls; active menu priority determines which action receives input.";
                    }
                    result.push_back(std::move(binding));
                }
            }
            return result;
        }
        std::string Lower(std::string text) {
            std::ranges::transform(text,text.begin(),[](unsigned char c) { return static_cast<char>(std::tolower(c)); }); return text;
        }
        class CatalogClient final : public Client {
        public:
            void Refresh() {
                auto next=providers.Read();
                auto game=GameBindings();
                next.sources.insert(next.sources.begin(),{"Fallout4","Fallout 4", "Live control tables: all 33 input contexts",game.size()});
                next.bindings.insert(next.bindings.begin(),game.begin(),game.end());
                auto mcm=McmRuntime::CatalogBindings();
                next.sources.push_back({"MCM","MCM","Keybind definitions and current saved assignments; activation scope unknown",mcm.size()});
                next.bindings.insert(next.bindings.end(),mcm.begin(),mcm.end());
                std::error_code error;
                for (const auto& file : std::filesystem::directory_iterator("Data/F4SE/Plugins",error)) {
                    if (Lower(file.path().extension().string())!=".dll") continue;
                    const auto name=Lower(file.path().stem().string());
                    if (std::ranges::any_of(next.sources,[&](const auto& source) { return Lower(source.id)==name; })) continue;
                    const bool loaded=::GetModuleHandleW(file.path().filename().c_str())!=nullptr;
                    next.undisclosed.push_back(file.path().filename().string() + (loaded ? "" : " (not loaded)"));
                }
                std::ranges::sort(next.undisclosed);
                next.conflicts=keys::Conflicts(next.bindings);
                auto shared=std::make_shared<const keys::Snapshot>(std::move(next));
                std::scoped_lock lock(mutex_); snapshot_=std::move(shared);
            }
            void OnFocusChanged(bool focused) override { if (focused) Refresh(); }
            void DrawSettingsNavigation(const FrameContext&) override { view_.Navigation(); }
            void Draw(const FrameContext&) override {
                std::shared_ptr<const keys::Snapshot> snapshot;
                { std::scoped_lock lock(mutex_); snapshot=snapshot_; }
                if (snapshot && view_.Draw(*snapshot)) QueueGameTask([this] { Refresh(); });
            }
        private:
            std::mutex mutex_;
            std::shared_ptr<const keys::Snapshot> snapshot_=std::make_shared<keys::Snapshot>();
            keys::View view_;
        } client;
    }
    bool Register(const B21UI_KeybindingProvider& provider) { return providers.Add(provider); }
    void Start() {
        b21ui::Register(client,{.name="UI21_Keybindings",.settings=true,.settingsLabel="Keybindings",.settingsIcon=modern::icon::Keyboard});
    }
}
