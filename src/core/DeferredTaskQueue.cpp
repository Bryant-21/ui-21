#include "core/DeferredTaskQueue.h"

#include <utility>

namespace b21ui::core {
    DeferredTaskQueue::DeferredTaskQueue(Submit submit)
        : worker_([this, submit = std::move(submit)](std::stop_token stop) {
            while (!stop.stop_requested()) {
                Task task;
                {
                    std::unique_lock lock(mutex_);
                    if (!ready_.wait(lock, stop, [this] { return !pending_.empty(); })) return;
                    task = std::move(pending_.front());
                    pending_.pop_front();
                }
                submit(std::move(task));
            }
        }) {}

    void DeferredTaskQueue::Post(Task task) {
        {
            std::scoped_lock lock(mutex_);
            pending_.push_back(std::move(task));
        }
        ready_.notify_one();
    }
}
