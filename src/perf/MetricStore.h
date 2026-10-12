#pragma once
#include "perf/PerfMath.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace b21ui::perf {
    struct MetricMeta {
        std::string id, owner, label, group;
        std::uint32_t unit{}, flags{};
    };

    // Values pushed in one frame: NaN where a metric got no number this frame.
    struct FrameRow {
        std::vector<float> values;
        std::vector<std::pair<std::uint32_t, std::string>> texts;  // metric index, new text
    };

    // Registration and pushes come from any thread; CloseFrame and reads too. Each metric has its own
    // short lock, so a push never waits on another metric or on a reader of the registry.
    class MetricStore {
    public:
        static constexpr std::uint32_t kMaxMetrics = 1024;
        static constexpr std::uint32_t kHistory = 1024;

        MetricStore();
        ~MetricStore();

        // Idempotent by id; returns the 1-based handle, or 0 for an invalid id or a full store.
        std::uint32_t Register(std::string_view id, std::string_view label, std::string_view group, std::uint32_t unit,
                               std::uint32_t flags);
        std::optional<std::uint32_t> Find(std::string_view id) const;
        void Push(std::uint32_t handle, double value);
        void PushText(std::uint32_t handle, std::string_view text);
        // Moves this frame's pushes into each metric's history; fills `row` when given. `allTexts` puts
        // every text metric's value in the row, changed or not (a capture's first row).
        void CloseFrame(FrameRow* row, bool allTexts = false);

        std::uint32_t Count() const { return count_.load(std::memory_order_acquire); }
        const MetricMeta* Meta(std::uint32_t handle) const;

        struct Snapshot {
            bool hasValue{};
            double current{};
            Stats stats;
            std::string text;
        };
        bool Read(std::uint32_t handle, Snapshot& out) const;
        // Oldest first; the newest `capacity` samples when the history holds more.
        std::uint32_t History(std::uint32_t handle, float* out, std::uint32_t capacity) const;
        std::vector<float> History(std::uint32_t handle) const;
        // Newest sample, or NaN.
        double Current(std::uint32_t handle) const;

    private:
        struct Entry;
        Entry* At(std::uint32_t handle) const;

        mutable std::mutex registry_;
        std::unordered_map<std::string, std::uint32_t> byId_;
        std::array<std::unique_ptr<Entry>, kMaxMetrics> entries_;
        std::atomic<std::uint32_t> count_{};
    };
}
