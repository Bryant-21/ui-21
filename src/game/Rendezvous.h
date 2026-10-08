#pragma once
#include "b21ui/Abi.h"

namespace b21ui::game {
    const B21UI_HostApi* HostApiTable();   // this module's table (Host.cpp)
    void StartHost();                      // Host.cpp; only called when IsHost()
    const B21UI_HostApi* Rendezvous();     // cached after the first call
    bool IsHost();
}
