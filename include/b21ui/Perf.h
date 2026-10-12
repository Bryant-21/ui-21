#pragma once
#include "b21ui/Abi.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// One performance overlay, page and capture for every B21UI mod. The elected host measures frame
// time, GPU frame time, CPU, RAM and VRAM itself; mods add their own metrics here and do that work
// only while Active() is true.
namespace b21ui::perf {
    enum class Unit : std::uint32_t {
        Ms = B21UI_UNIT_MS, Count = B21UI_UNIT_COUNT, MB = B21UI_UNIT_MB, Percent = B21UI_UNIT_PERCENT,
        Fps = B21UI_UNIT_FPS, Ratio = B21UI_UNIT_RATIO, Text = B21UI_UNIT_TEXT
    };

    enum class Mode : std::uint32_t {
        Off = B21UI_PERF_MODE_OFF, Compact = B21UI_PERF_MODE_COMPACT,
        Expanded = B21UI_PERF_MODE_EXPANDED, Full = B21UI_PERF_MODE_FULL
    };

    struct MetricOptions {
        const char* group = "";
        // Pinned to the Expanded overlay until the user unpins it.
        bool headline = false;
    };

    // Construct at any time, including static init; registration made before the rendezvous is
    // queued. Strings are copied. Push from any thread.
    class Metric {
    public:
        Metric() = default;
        Metric(const char* id, const char* label, Unit unit, MetricOptions options = {});
        void Push(double value) const;
        void PushText(std::string_view text) const;

    private:
        std::uint32_t slot_ = 0;
    };

    // True while the overlay shows Expanded/Full, a capture is waiting or recording, or a
    // Performance page is on screen. Cheap; call it every frame before measuring.
    bool Active();
    bool Capturing();

    // Extra tabs drawn after the shared ones, in the caller's own ImGui context. They only appear
    // where this module draws the page, never in another mod's copy of it.
    struct PageTab {
        const char* label;
        std::function<void()> draw;
    };

    // The full Performance page (Live, Metrics, Capture, then extraTabs), drawn into the current
    // ImGui window. Every consumer compiles it; the host also shows it under MOD SETTINGS.
    void DrawPage(std::span<const PageTab> extraTabs = {});

    Mode CurrentMode();
    void SetMode(Mode mode);
    bool StartCapture();
    void StopCapture();
}
