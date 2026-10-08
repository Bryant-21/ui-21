#pragma once

#include "b21ui/Abi.h"

#include <functional>
#include <memory>
#include <utility>

namespace b21ui::core {
    inline std::function<void()> OwnedGameTask(void* user, B21UI_TaskCallback run, B21UI_TaskCallback destroy) {
        return [data = std::shared_ptr<void>(user, destroy), run] { run(data.get()); };
    }

    inline bool SubmitHostGameTask(const B21UI_HostApi* host, std::function<void()>& task) {
        if (!host || host->size < offsetof(B21UI_HostApi, queueGameTask) + sizeof(host->queueGameTask) ||
            !host->queueGameTask) return false;
        auto* user = new std::function<void()>(std::move(task));
        host->queueGameTask(user,
            +[](void* data) { (*static_cast<std::function<void()>*>(data))(); },
            +[](void* data) { delete static_cast<std::function<void()>*>(data); });
        return true;
    }
}
