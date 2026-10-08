#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace b21ui::core {
    class DeferredTaskQueue {
    public:
        using Task = std::function<void()>;
        using Submit = std::function<void(Task)>;

        explicit DeferredTaskQueue(Submit submit);
        void Post(Task task);

    private:
        std::mutex mutex_;
        std::condition_variable_any ready_;
        std::deque<Task> pending_;
        std::jthread worker_;
    };
}
