#pragma once
#include "b21ui/Abi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace b21ui::game {
    const B21UI_HostApi* HostApiTable();   // this module's table (Host.cpp)
    void StartHost();                      // Host.cpp; only called when IsHost()
    const B21UI_HostApi* Rendezvous();     // cached after the first call
    bool IsHost();
    struct RendezvousModule {
        std::string name;
        std::uint32_t frameworkVersion{};
        bool host{};
    };
    // Every module that answered the rendezvous, in load order.
    const std::vector<RendezvousModule>& RendezvousModules();
}
