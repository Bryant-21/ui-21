#include "perf/MetricStore.h"
#include "b21ui/Abi.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace b21ui::perf {
    struct MetricStore::Entry {
        MetricMeta meta;
        mutable std::mutex lock;
        double pending{};
        bool hasPending{};
        std::string pendingText;
        bool hasPendingText{};
        double current = std::numeric_limits<double>::quiet_NaN();
        bool hasValue{};
        std::string text;
        std::array<float, kHistory> ring{};
        std::uint32_t head{}, size{};
    };

    MetricStore::MetricStore() = default;
    MetricStore::~MetricStore() = default;

    MetricStore::Entry* MetricStore::At(std::uint32_t handle) const {
        return handle >= 1 && handle <= count_.load(std::memory_order_acquire) ? entries_[handle - 1].get() : nullptr;
    }

    std::uint32_t MetricStore::Register(std::string_view id, std::string_view label, std::string_view group,
                                        std::uint32_t unit, std::uint32_t flags) {
        if (!ValidId(id)) return 0;
        std::scoped_lock guard(registry_);
        if (const auto found = byId_.find(std::string(id)); found != byId_.end()) return found->second;
        const auto index = count_.load(std::memory_order_relaxed);
        if (index >= kMaxMetrics) return 0;
        auto entry = std::make_unique<Entry>();
        entry->meta = {std::string(id), std::string(OwnerOf(id)), label.empty() ? std::string(id) : std::string(label),
                       std::string(group), unit, flags};
        entries_[index] = std::move(entry);
        byId_.emplace(std::string(id), index + 1);
        count_.store(index + 1, std::memory_order_release);
        return index + 1;
    }

    std::optional<std::uint32_t> MetricStore::Find(std::string_view id) const {
        std::scoped_lock guard(registry_);
        const auto found = byId_.find(std::string(id));
        return found == byId_.end() ? std::nullopt : std::optional(found->second);
    }

    const MetricMeta* MetricStore::Meta(std::uint32_t handle) const {
        const auto* entry = At(handle);
        return entry ? &entry->meta : nullptr;
    }

    void MetricStore::Push(std::uint32_t handle, double value) {
        auto* entry = At(handle);
        if (!entry || !std::isfinite(value)) return;
        std::scoped_lock guard(entry->lock);
        entry->pending = value;
        entry->hasPending = true;
    }

    void MetricStore::PushText(std::uint32_t handle, std::string_view text) {
        auto* entry = At(handle);
        if (!entry) return;
        std::scoped_lock guard(entry->lock);
        entry->pendingText.assign(text);
        entry->hasPendingText = true;
    }

    void MetricStore::CloseFrame(FrameRow* row, bool allTexts) {
        const auto count = Count();
        if (row) {
            row->values.assign(count, std::numeric_limits<float>::quiet_NaN());
            row->texts.clear();
        }
        for (std::uint32_t i = 0; i < count; ++i) {
            auto& e = *entries_[i];
            std::scoped_lock guard(e.lock);
            if (e.hasPending) {
                e.hasPending = false;
                e.current = e.pending;
                e.hasValue = true;
                e.ring[e.head] = static_cast<float>(e.pending);
                e.head = (e.head + 1) % kHistory;
                e.size = std::min(e.size + 1, kHistory);
                if (row) row->values[i] = static_cast<float>(e.pending);
            }
            const bool changed = e.hasPendingText && e.pendingText != e.text;
            if (e.hasPendingText) {
                e.hasPendingText = false;
                e.text.swap(e.pendingText);
                e.hasValue = true;
            }
            if (row && (changed || (allTexts && e.meta.unit == B21UI_UNIT_TEXT && e.hasValue))) row->texts.emplace_back(i, e.text);
        }
    }

    std::uint32_t MetricStore::History(std::uint32_t handle, float* out, std::uint32_t capacity) const {
        const auto* entry = At(handle);
        if (!entry || !out) return 0;
        std::scoped_lock guard(entry->lock);
        const auto n = std::min(capacity, entry->size);
        const auto start = (entry->head + kHistory - n) % kHistory;
        for (std::uint32_t i = 0; i < n; ++i) out[i] = entry->ring[(start + i) % kHistory];
        return n;
    }

    std::vector<float> MetricStore::History(std::uint32_t handle) const {
        std::vector<float> out(kHistory);
        out.resize(History(handle, out.data(), kHistory));
        return out;
    }

    double MetricStore::Current(std::uint32_t handle) const {
        const auto* entry = At(handle);
        if (!entry) return std::numeric_limits<double>::quiet_NaN();
        std::scoped_lock guard(entry->lock);
        return entry->current;
    }

    bool MetricStore::Read(std::uint32_t handle, Snapshot& out) const {
        const auto* entry = At(handle);
        if (!entry) return false;
        std::array<float, kHistory> copy;
        std::uint32_t n{};
        {
            std::scoped_lock guard(entry->lock);
            out.hasValue = entry->hasValue;
            out.current = entry->current;
            out.text = entry->text;
            n = entry->size;
            const auto start = (entry->head + kHistory - n) % kHistory;
            for (std::uint32_t i = 0; i < n; ++i) copy[i] = entry->ring[(start + i) % kHistory];
        }
        out.stats = Compute(std::span<const float>(copy.data(), n));
        return true;
    }
}
