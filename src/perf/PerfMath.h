#pragma once
#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Pure helpers shared by the host store, the capture writer and the page (no Windows, no ImGui).
namespace b21ui::perf {
    inline constexpr const char* kFrameMs = "b21ui.frame.ms";
    inline constexpr const char* kFrameFps = "b21ui.frame.fps";
    inline constexpr const char* kGpuMs = "b21ui.gpu.ms";
    inline constexpr const char* kCpuPercent = "b21ui.cpu.percent";
    inline constexpr const char* kRamMb = "b21ui.mem.ram_mb";
    inline constexpr const char* kVramMb = "b21ui.mem.vram_mb";

    // The text before the first dot; the whole id when it has none.
    std::string_view OwnerOf(std::string_view id);
    // Ids are stored in comma lists and CSV headers: printable ASCII without commas, quotes or spaces.
    bool ValidId(std::string_view id);

    struct Stats {
        double average{}, minimum{}, maximum{}, p50{}, p99{};
        std::size_t samples{};
    };
    // q in [0, 1]; nearest-rank on a copy. Empty input gives 0.
    double Percentile(std::span<const float> values, double q);
    Stats Compute(std::span<const float> values);

    // 1% and 0.1% lows as FPS: 1000 / the 99th and 99.9th percentile frame time.
    struct Lows {
        double low1Fps{}, low01Fps{};
    };
    Lows FrameLows(std::span<const float> frameMs);

    std::vector<std::string> SplitList(std::string_view text);
    std::string JoinList(const std::set<std::string>& items);

    // The user's pin choices relative to each metric's headline default.
    class PinOverrides {
    public:
        bool IsPinned(const std::string& id, bool headline) const;
        // Returns true when the stored lists changed.
        bool Set(const std::string& id, bool headline, bool pinned);
        void Load(std::string_view pinnedList, std::string_view unpinnedList);
        std::string PinnedList() const { return JoinList(pinned_); }
        std::string UnpinnedList() const { return JoinList(unpinned_); }

    private:
        std::set<std::string> pinned_, unpinned_;
    };

    const char* UnitName(std::uint32_t unit);
    // "12.34 ms", "61.2 FPS", "512 MB"...; text metrics are shown as their text instead.
    std::string FormatValue(double value, std::uint32_t unit);
}
