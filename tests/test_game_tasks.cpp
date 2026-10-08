#include <doctest/doctest.h>
#include "core/GameTasks.h"

namespace {
    std::function<void()> pendingTask;
}

TEST_CASE("host tasks invoke the caller's disposer once after execution or discard") {
    struct Counts { int runs{}, disposed{}; } counts;
    B21UI_HostApi host{};
    host.size = sizeof(host);
    host.queueGameTask = +[](void* user, B21UI_TaskCallback run, B21UI_TaskCallback destroy) {
        pendingTask = b21ui::core::OwnedGameTask(user, run, destroy);
    };
    auto lifetime = std::shared_ptr<int>(new int{}, [&counts](int* user) { ++counts.disposed; delete user; });
    std::function<void()> submitted = [lifetime, &counts] { ++counts.runs; };
    lifetime.reset();
    REQUIRE(b21ui::core::SubmitHostGameTask(&host, submitted));
    submitted = {};
    {
        auto copy = pendingTask;
        pendingTask = {};
        SUBCASE("execute") { copy(); CHECK(counts.runs == 1); }
        SUBCASE("discard") { CHECK(counts.runs == 0); }
        CHECK(counts.disposed == 0);
    }
    CHECK(counts.disposed == 1);
}

TEST_CASE("older or absent hosts leave the task intact for safe local submission") {
    int runs = 0;
    std::function<void()> task = [&] { ++runs; };
    B21UI_HostApi host{};
    host.size = sizeof(host);
    const B21UI_HostApi* api = &host;
    SUBCASE("no elected host") { api = nullptr; }
    SUBCASE("v5 host without the appended field") {
        host.size = offsetof(B21UI_HostApi, queueGameTask);
        host.queueGameTask = +[](void* user, B21UI_TaskCallback run, B21UI_TaskCallback destroy) {
            run(user);
            destroy(user);
        };
    }
    SUBCASE("host with no submission callback") {}
    CHECK_FALSE(b21ui::core::SubmitHostGameTask(api, task));
    REQUIRE(static_cast<bool>(task));
    task();
    CHECK(runs == 1);
}
