#pragma once

#include "b21ui/Abi.h"

#include <functional>

namespace b21ui::game {
    void SetGameTaskHost(const B21UI_HostApi* host);
    void QueueLocalGameTask(std::function<void()> task);
}
