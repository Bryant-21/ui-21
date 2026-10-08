#include <doctest/doctest.h>
#include "core/DeferredTaskQueue.h"
#include "core/GameTasks.h"

#include <chrono>
#include <future>
#include <semaphore>
#include <vector>

using b21ui::core::DeferredTaskQueue;
using namespace std::chrono_literals;

namespace {
    DeferredTaskQueue* hostQueue{};
    void QueueOnHost(void* user, B21UI_TaskCallback run, B21UI_TaskCallback destroy) {
        hostQueue->Post(b21ui::core::OwnedGameTask(user, run, destroy));
    }
}

TEST_CASE("clients sharing a host continue posting while the engine task queue is blocked") {
    std::binary_semaphore release{0};
    std::promise<void> entered, completed;
    auto blocked = entered.get_future();
    auto drained = completed.get_future();
    int submissions = 0;
    std::vector<int> order;
    DeferredTaskQueue queue{[&](DeferredTaskQueue::Task task) {
        if (++submissions == 1) {
            entered.set_value();
            release.acquire();
        }
        task();
    }};
    hostQueue = &queue;
    B21UI_HostApi host{};
    host.size = sizeof(host);
    host.queueGameTask = &QueueOnHost;
    std::function<void()> first = [&] { order.push_back(1); };
    REQUIRE(b21ui::core::SubmitHostGameTask(&host, first));
    const auto started = blocked.wait_for(2s);
    auto render = std::async(std::launch::async, [&] {
        std::function<void()> second = [&] { order.push_back(2); };
        std::function<void()> third = [&] { order.push_back(3); completed.set_value(); };
        return b21ui::core::SubmitHostGameTask(&host, second) && b21ui::core::SubmitHostGameTask(&host, third);
    });
    const auto submitted = render.wait_for(200ms);
    release.release();
    CHECK(render.get());
    CHECK(started == std::future_status::ready);
    CHECK(submitted == std::future_status::ready);
    REQUIRE(drained.wait_for(2s) == std::future_status::ready);
    CHECK(order == std::vector<int>{1, 2, 3});
    hostQueue = nullptr;
}

TEST_CASE("a submitted engine task can post another task without holding the local queue lock") {
    std::promise<void> completed;
    auto done = completed.get_future();
    DeferredTaskQueue queue{[](DeferredTaskQueue::Task task) { task(); }};
    queue.Post([&] { queue.Post([&] { completed.set_value(); }); });
    CHECK(done.wait_for(2s) == std::future_status::ready);
}
