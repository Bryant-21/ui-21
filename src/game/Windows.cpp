#include <F4SE/F4SE.h>

#include "b21ui/B21UI.h"
#include "b21ui/Tasks.h"
#include "b21ui/Windows.h"

#include <spdlog/spdlog.h>

#include <Windows.h>

namespace b21ui {
    namespace {
        const Window* FindInstalled(std::string_view id) {
            for (const auto* window : InstalledWindows({}))
                if (window->id == id) return window;
            return nullptr;
        }

        void Send(const Window& window) {
            QueueGameTask([plugin = window.plugin] {
                auto* messaging = F4SE::GetMessagingInterface();
                if (!messaging || !messaging->Dispatch(kOpenWindowMessage, nullptr, 0, plugin))
                    spdlog::warn("B21UI: could not ask {} to open its window", plugin);
            });
        }
    }

    std::vector<const Window*> InstalledWindows(std::string_view self) {
        return InstalledWindows(self, [](const Window& window) { return ::GetModuleHandleW(window.module) != nullptr; });
    }

    bool OpenWindow(std::string_view id) {
        const auto* window = FindInstalled(id);
        if (window) Send(*window);
        return window != nullptr;
    }

    bool SwitchToWindow(Client& self, std::string_view id) {
        const auto* window = FindInstalled(id);
        if (!window) return false;
        Close(self);
        Send(*window);
        return true;
    }
}
