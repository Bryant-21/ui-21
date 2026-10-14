#include "perf/PerfMath.h"
#include "b21ui/Abi.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace b21ui::perf {
    std::string_view OwnerOf(std::string_view id) {
        const auto dot = id.find('.');
        return dot == std::string_view::npos ? id : id.substr(0, dot);
    }

    bool ValidId(std::string_view id) {
        if (id.empty() || id.size() > 128 || id.front() == '.') return false;
        return std::ranges::all_of(id, [](char c) { return c > ' ' && c < 0x7F && c != ',' && c != '"' && c != ';'; });
    }

    double Percentile(std::span<const float> values, double q) {
        if (values.empty()) return 0.0;
        std::vector<float> copy(values.begin(), values.end());
        const auto rank = static_cast<std::size_t>(std::ceil(std::clamp(q, 0.0, 1.0) * static_cast<double>(copy.size())));
        const auto index = rank == 0 ? 0 : rank - 1;
        std::ranges::nth_element(copy, copy.begin() + static_cast<std::ptrdiff_t>(index));
        return copy[index];
    }

    Stats Compute(std::span<const float> values) {
        Stats stats;
        if (values.empty()) return stats;
        double sum = 0.0;
        stats.minimum = stats.maximum = values.front();
        for (const auto v : values) {
            sum += v;
            stats.minimum = std::min<double>(stats.minimum, v);
            stats.maximum = std::max<double>(stats.maximum, v);
        }
        stats.samples = values.size();
        stats.average = sum / static_cast<double>(values.size());
        stats.p50 = Percentile(values, 0.50);
        stats.p99 = Percentile(values, 0.99);
        return stats;
    }

    Lows FrameLows(std::span<const float> frameMs) {
        const auto p99 = Percentile(frameMs, 0.99), p999 = Percentile(frameMs, 0.999);
        return {p99 > 0.0 ? 1000.0 / p99 : 0.0, p999 > 0.0 ? 1000.0 / p999 : 0.0};
    }

    std::vector<std::string> SplitList(std::string_view text) {
        std::vector<std::string> out;
        while (!text.empty()) {
            const auto comma = text.find(',');
            auto item = text.substr(0, comma);
            while (!item.empty() && item.front() == ' ') item.remove_prefix(1);
            while (!item.empty() && item.back() == ' ') item.remove_suffix(1);
            if (ValidId(item)) out.emplace_back(item);
            if (comma == std::string_view::npos) break;
            text.remove_prefix(comma + 1);
        }
        return out;
    }

    std::string JoinList(const std::set<std::string>& items) {
        std::string out;
        for (const auto& item : items) {
            if (!out.empty()) out += ',';
            out += item;
        }
        return out;
    }

    bool PinOverrides::IsPinned(const std::string& id, bool headline) const {
        return headline ? !unpinned_.contains(id) : pinned_.contains(id);
    }

    bool PinOverrides::Set(const std::string& id, bool headline, bool pinned) {
        if (!ValidId(id)) return false;
        const bool wasPinned = pinned_.contains(id), wasUnpinned = unpinned_.contains(id);
        pinned_.erase(id);
        unpinned_.erase(id);
        if (pinned != headline) (pinned ? pinned_ : unpinned_).insert(id);
        return wasPinned != pinned_.contains(id) || wasUnpinned != unpinned_.contains(id);
    }

    void PinOverrides::Load(std::string_view pinnedList, std::string_view unpinnedList) {
        pinned_.clear();
        unpinned_.clear();
        for (auto& id : SplitList(pinnedList)) pinned_.insert(std::move(id));
        for (auto& id : SplitList(unpinnedList))
            if (!pinned_.contains(id)) unpinned_.insert(std::move(id));
    }

    const char* UnitName(std::uint32_t unit) {
        switch (unit) {
        case B21UI_UNIT_MS: return "ms";
        case B21UI_UNIT_COUNT: return "count";
        case B21UI_UNIT_MB: return "MB";
        case B21UI_UNIT_PERCENT: return "percent";
        case B21UI_UNIT_FPS: return "fps";
        case B21UI_UNIT_RATIO: return "ratio";
        case B21UI_UNIT_TEXT: return "text";
        default: return "unknown";
        }
    }

    std::string FormatValue(double value, std::uint32_t unit) {
        if (!std::isfinite(value)) return "n/a";
        switch (unit) {
        case B21UI_UNIT_MS: return std::format("{:.2f} ms", value);
        case B21UI_UNIT_COUNT: return std::abs(value - std::round(value)) < 1e-9 ? std::format("{:.0f}", value) : std::format("{:.1f}", value);
        case B21UI_UNIT_MB: return std::format("{:.0f} MB", value);
        case B21UI_UNIT_PERCENT: return std::format("{:.1f}%", value);
        case B21UI_UNIT_FPS: return std::format("{:.1f} FPS", value);
        case B21UI_UNIT_RATIO: return std::format("{:.3f}", value);
        default: return std::format("{:.3g}", value);
        }
    }

    std::uint32_t OverlayColumns(float available, float columnWidth) {
        if (!(columnWidth > 0.0F) || !(available > columnWidth)) return 1;
        return static_cast<std::uint32_t>(available / columnWidth);
    }

    std::size_t RowsPerColumn(std::size_t lines, std::uint32_t columns) {
        const std::size_t n = std::max<std::uint32_t>(columns, 1);
        return (lines + n - 1) / n;
    }

    float BoundedListHeight(float available, float viewportHeight, float minHeight) {
        const float cap = std::max(minHeight, viewportHeight * kListViewportShare);
        return std::clamp(available, minHeight, cap);
    }

    std::vector<float> BinByTime(std::span<const float> values, std::span<const double> times, double end, double window,
                                 std::size_t bins) {
        const auto n = std::min(values.size(), times.size());
        if (bins == 0 || !(window > 0.0)) return {};
        const double width = window / static_cast<double>(bins);
        // Bin edges sit on a fixed time grid: edges that slid with `end` every frame moved samples
        // between bins each redraw, and the whole trace shimmered.
        end = std::ceil(end / width - 1e-6) * width;
        const double start = end - window;
        std::vector<double> sums(bins, 0.0);
        std::vector<std::uint32_t> counts(bins, 0);
        bool any = false;
        for (std::size_t i = 0; i < n; ++i) {
            if (!(times[i] > start) || times[i] > end || !std::isfinite(values[i])) continue;
            const auto bin = std::min(bins - 1, static_cast<std::size_t>((times[i] - start) / width));
            sums[bin] += values[i];
            ++counts[bin];
            any = true;
        }
        if (!any) return {};
        std::vector<float> out(bins);
        float held = 0.0F;
        for (std::size_t i = 0; i < bins; ++i)
            if (counts[i]) {
                held = static_cast<float>(sums[i] / counts[i]);
                break;
            }
        for (std::size_t i = 0; i < bins; ++i) {
            if (counts[i]) held = static_cast<float>(sums[i] / counts[i]);
            out[i] = held;
        }
        return out;
    }

    float GraphTop(float maximum, float step) {
        if (!(step > 0.0F)) step = 1.0F;
        const float steps = std::isfinite(maximum) ? std::ceil(maximum / step) : 1.0F;
        return step * std::max(1.0F, steps);
    }
}
