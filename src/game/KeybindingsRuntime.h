#pragma once
#include "b21ui/Abi.h"
namespace b21ui::game::KeybindingsRuntime {
    bool Register(const B21UI_KeybindingProvider& provider);
    void Start();
}
