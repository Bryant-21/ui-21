#include "b21ui/Tasks.h"
#include "core/DeferredTaskQueue.h"
#include "core/GameTasks.h"
#include "game/Tasks.h"

#include <F4SE/F4SE.h>

#include <utility>
#include <atomic>

namespace b21ui::game {
    namespace {
        std::atomic<const B21UI_HostApi*> taskHost{};
    }

    void SetGameTaskHost(const B21UI_HostApi* host) {
        taskHost.store(host);
    }

    void QueueLocalGameTask(std::function<void()> task) {
        static core::DeferredTaskQueue queue{[](core::DeferredTaskQueue::Task next) {
            if (auto* tasks = F4SE::GetTaskInterface()) tasks->AddTask(std::move(next));
        }};
        queue.Post(std::move(task));
    }
}

namespace b21ui {
    void QueueGameTask(std::function<void()> task) {
        if (!core::SubmitHostGameTask(game::taskHost.load(), task)) game::QueueLocalGameTask(std::move(task));
    }
}
