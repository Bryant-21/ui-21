#include <F4SE/F4SE.h>
#include <RE/Fallout.h>

#include "game/PerfRuntime.h"
#include "game/Rendezvous.h"
#include "b21ui/Keybindings.h"
#include "perf/PerfHost.h"

#include <Windows.h>

namespace b21ui::game::PerfRuntime {
    namespace {
        constexpr int kF11 = 0x57, kF12 = 0x58;

        std::vector<keys::Binding> Bindings() {
            const auto binding = [](const char* id, const char* label, int key, const char* conditions) {
                keys::Binding b{id, label, "B21UI", key, 1, true};
                b.contexts = {"*"};
                b.trigger = "press";
                b.conditions = conditions;
                return b;
            };
            return {binding("perfOverlay", "Performance overlay: cycle Off / Compact / Expanded / Full", kF11,
                            "Not while a B21UI window has focus."),
                    binding("perfCapture", "Performance capture: start / stop", kF12, "Not while a B21UI window has focus.")};
        }
    }

    void Start() {
        auto mods = nlohmann::json::array();
        for (const auto& module : RendezvousModules())
            mods.push_back({{"module", module.name}, {"frameworkVersion", module.frameworkVersion}, {"host", module.host}});
        perf::Host::Get().Start({{"mods", std::move(mods)}});
        keys::Register("B21UI", "B21UI", &Bindings);
    }

    void OnButton(const RE::ButtonEvent& event) {
        if (event.device != RE::INPUT_DEVICE::kKeyboard || event.value <= 0.0F || event.heldDownSecs != 0.0F) return;
        const auto code = static_cast<int>(event.idCode);
        if (code != kF11 && code != kF12) return;
        const bool shift = (::GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool other = (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 || (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        if (!shift || other) return;
        auto& host = perf::Host::Get();
        if (code == kF11) host.CycleMode();
        else host.ToggleCapture();
    }
}
