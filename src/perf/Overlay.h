#pragma once
#include "b21ui/B21UI.h"

namespace b21ui::perf {
    // The input-transparent overlay the host opens while the mode is not Off.
    Client& OverlayClient();
    // Registers the overlay and the MOD SETTINGS "Performance" page with this module's B21UI.
    void RegisterPerfClients();
}
