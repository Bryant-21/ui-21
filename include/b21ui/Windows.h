#pragma once

#include <functional>
#include <span>
#include <string_view>
#include <vector>

namespace b21ui {
    class Client;

    // A B21 window every B21 modern UI can open. Mod windows use kOpenWindowMessage; shared settings
    // route through the elected host.
    struct Window {
        std::string_view id;
        std::string_view label;
        const char* glyph;      // Font Awesome Solid, UTF-8
        const wchar_t* module;  // null for the shared settings host
        const char* plugin;     // F4SE plugin name, or null for shared settings
    };

    std::span<const Window> Windows();
    // Catalog windows `loaded` accepts, without `self`.
    std::vector<const Window*> InstalledWindows(std::string_view self, const std::function<bool(const Window&)>& loaded);
    // Catalog windows whose DLL is loaded, without `self`. The desktop preview counts every window as loaded.
    std::vector<const Window*> InstalledWindows(std::string_view self);
    // Asks the window's mod to open it, from the game thread. False when it isn't installed.
    bool OpenWindow(std::string_view id);
    // Closes `self`, then opens `id`. Both run in order on the game thread, so a modal target finds the
    // screen free. False, leaving `self` open, when `id` isn't installed.
    bool SwitchToWindow(Client& self, std::string_view id);
}
