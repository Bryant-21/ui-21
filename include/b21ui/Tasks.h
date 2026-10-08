#pragma once

#include <functional>

namespace b21ui {
    // F4SE can execute tasks while holding a lock whose owner waits on rendering. Submitting
    // through the elected host's worker prevents rendering from completing that lock cycle.
    void QueueGameTask(std::function<void()> task);
}
