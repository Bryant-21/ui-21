#pragma once

namespace RE { class ButtonEvent; }

namespace b21ui::game::PerfRuntime {
    // Host only: starts the performance service and publishes its hotkeys to the keybinding catalog.
    void Start();
    // Shift+F11 cycles the overlay, Shift+F12 starts or stops a capture. Input thread.
    void OnButton(const RE::ButtonEvent& event);
}
