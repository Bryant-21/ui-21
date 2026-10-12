#pragma once
#include "b21ui/Abi.h"

namespace b21ui::client {
    // The table b21ui::perf calls (the elected host's, or the preview's). Metrics constructed before
    // this are registered now.
    void SetPerfHostApi(const B21UI_HostApi* host);
}
