#pragma once
#include "b21ui/Keybindings.h"
namespace RE { class ButtonEvent; }
namespace b21ui::game::McmRuntime {
    void RegisterPapyrus();
    void Start();
    void OnButton(const RE::ButtonEvent& event);
    std::vector<keys::Binding> CatalogBindings();
}
