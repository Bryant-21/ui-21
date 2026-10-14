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

    // The Expanded overlay's rows: every pinned metric, in registration order, grouped by owner (owners in
    // first-seen order). The host's own metrics come first with no header; the frame time and FPS sit in
    // the headline and are never repeated. `index` points into the input; a header row names its owner.
    struct PinView {
        std::string_view id, owner;
        bool pinned{};
    };
    struct OverlayRow {
        std::size_t index{};
        bool header{};
    };
    std::vector<OverlayRow> PinnedRows(std::span<const PinView> metrics);

    // The full overlay's metric columns: as many as the overlay width holds, at least one.
    std::uint32_t OverlayColumns(float available, float columnWidth);
    std::size_t RowsPerColumn(std::size_t lines, std::uint32_t columns);

    // A scrolling list's height. Inside a parent that grows to fit its content the room
    // left reported to a fill-the-rest child grows with it every frame, so the list is
    // held to a share of the screen.
    inline constexpr float kListViewportShare = 0.6F;
    float BoundedListHeight(float available, float viewportHeight, float minHeight);

    // Realtime graphs: the samples in (end - window, end] averaged into `bins` equal time slots, so
    // the trace scrolls at one speed whatever the frame rate and single-frame jitter is blended.
    // Only finished slots on a fixed time grid are returned (`end` rounds down to a slot edge). A history
    // shorter than the window starts at its first whole slot, so the result can be shorter than `bins`
    // and the plot still spans its box. An empty slot holds the value before it. Empty when no sample is in.
    std::vector<float> BinByTime(std::span<const float> values, std::span<const double> times, double end, double window,
                                 std::size_t bins);
    inline constexpr double kGraphWindowSeconds = 10.0;
    inline constexpr std::size_t kGraphBins = 600;
    // The graph's top: the smallest whole multiple of `step` (1 when step is not positive) that
    // holds `maximum`, so the scale only moves when the window's peak crosses a step.
    float GraphTop(float maximum, float step);

    const char* UnitName(std::uint32_t unit);
    // "12.34 ms", "61.2 FPS", "512 MB"...; text metrics are shown as their text instead.
    std::string FormatValue(double value, std::uint32_t unit);
}
