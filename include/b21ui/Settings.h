#pragma once
#include "b21ui/Abi.h"

#include <vector>

namespace b21ui {
    std::vector<B21UI_SettingsPanel> SettingsPanels();
    // Zero reopens the last selected panel, or the first registered panel.
    bool OpenSettings(B21UI_ClientId id = 0);
    const char* SettingsCategory(B21UI_ClientId id);
}
