#include "b21ui/Perf.h"
#include "b21ui/Common.h"
#include "b21ui/modern/Theme.h"
#include "b21ui/modern/Widgets.h"
#include "client/PerfClient.h"
#include "perf/PerfHost.h"
#include "perf/PerfMath.h"

#include <imgui.h>
#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <mutex>

#define B21UI_HOST_HAS(h, field) ((h) && (h)->size >= offsetof(B21UI_HostApi, field) + sizeof((h)->field) && (h)->field)

namespace b21ui::perf {
    namespace {
        namespace theme = modern::theme;
        namespace w = modern::w;

        std::atomic<const B21UI_HostApi*> hostApi{};
        const B21UI_HostApi* Api() { return hostApi.load(std::memory_order_acquire); }

        // Metrics constructed in this module; each resolves to a host handle once a host exists.
        struct Local {
            std::string id, label, group;
            std::uint32_t unit{}, flags{};
            std::atomic<B21UI_MetricId> handle{};
            std::atomic<bool> refused{};
        };
        constexpr std::uint32_t kMaxLocal = 1024;
        struct Registry {
            std::mutex mutex;
            std::array<std::unique_ptr<Local>, kMaxLocal> items;
            std::atomic<std::uint32_t> count{};
        };
        Registry& Locals() {
            static Registry registry;
            return registry;
        }

        B21UI_MetricId Resolve(Local& local) {
            if (const auto handle = local.handle.load(std::memory_order_acquire); handle || local.refused.load()) return handle;
            const auto* h = Api();
            if (!B21UI_HOST_HAS(h, perfRegisterMetric)) return 0;
            std::scoped_lock guard(Locals().mutex);
            if (const auto handle = local.handle.load()) return handle;
            const B21UI_MetricDesc desc{sizeof(desc), local.id.c_str(), local.label.c_str(), local.group.c_str(), local.unit, local.flags};
            const auto handle = h->perfRegisterMetric(&desc);
            if (!handle) local.refused = true;
            local.handle.store(handle, std::memory_order_release);
            return handle;
        }

        Local* At(std::uint32_t slot) {
            return slot >= 1 && slot <= Locals().count.load(std::memory_order_acquire) ? Locals().items[slot - 1].get() : nullptr;
        }

        std::string ToUtf8(const std::filesystem::path& path) {
            const auto text = path.u8string();
            return {text.begin(), text.end()};
        }
        std::filesystem::path FromUtf8(std::string_view text) { return std::filesystem::path(std::u8string(text.begin(), text.end())); }

        std::string Lower(std::string_view text) {
            std::string out(text);
            std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return out;
        }

        struct Row {
            B21UI_MetricId id{};
            std::string key, owner, label, group, text;
            std::uint32_t unit{}, flags{};
            bool pinned{}, hasValue{}, hasBaseline{};
            double current{}, average{}, minimum{}, maximum{}, p50{}, p99{}, baseline{};
        };

        std::vector<Row> ReadRows(const B21UI_HostApi* h) {
            const auto total = h->perfMetrics(nullptr, 0);
            std::vector<B21UI_MetricInfo> infos(total);
            for (auto& info : infos) info.size = sizeof(info);
            const auto written = total ? std::min(total, h->perfMetrics(infos.data(), total)) : 0u;
            std::vector<Row> rows;
            rows.reserve(written);
            const auto str = [](const char* s) { return std::string(s ? s : ""); };
            for (std::uint32_t i = 0; i < written; ++i) {
                const auto& m = infos[i];
                rows.push_back({m.id, str(m.key), str(m.owner), str(m.label), str(m.group), str(m.text), m.unit, m.flags,
                                m.pinned != 0, m.hasValue != 0, m.hasBaseline != 0, m.current, m.average, m.minimum, m.maximum,
                                m.p50, m.p99, m.baselineAverage});
            }
            return rows;
        }

        std::vector<float> History(const B21UI_HostApi* h, B21UI_MetricId id) {
            std::vector<float> values(1024);
            values.resize(id ? h->perfHistory(id, values.data(), static_cast<std::uint32_t>(values.size())) : 0);
            return values;
        }

        // The last kGraphWindowSeconds binned by time; the raw samples on a host without timed history.
        std::vector<float> GraphSeries(const B21UI_HostApi* h, B21UI_MetricId id, const std::vector<float>& raw) {
            if (!id || !B21UI_HOST_HAS(h, perfHistoryTimed) || !B21UI_HOST_HAS(h, perfNow)) return raw;
            std::vector<float> values(MetricStore::kHistory);
            std::vector<double> times(MetricStore::kHistory);
            const auto n = h->perfHistoryTimed(id, values.data(), times.data(), static_cast<std::uint32_t>(values.size()));
            values.resize(n);
            times.resize(n);
            return BinByTime(values, times, h->perfNow(), kGraphWindowSeconds, kGraphBins);
        }

        const Row* Find(const std::vector<Row>& rows, std::string_view key) {
            const auto found = std::ranges::find(rows, key, &Row::key);
            return found == rows.end() ? nullptr : &*found;
        }

        float ListHeight() {
            return BoundedListHeight(ImGui::GetContentRegionAvail().y, ImGui::GetMainViewport()->Size.y, theme::Px(200));
        }

        B21UI_PerfSettings GetSettings(const B21UI_HostApi* h) {
            B21UI_PerfSettings s{};
            s.size = sizeof(s);
            h->perfGetSettings(&s);
            return s;
        }

        struct CaptureFile {
            std::filesystem::path json;
            std::string name;
        };
        struct PageState {
            char search[128]{};
            std::vector<CaptureFile> files;
            double filesAt = -1e9;
            std::uint32_t lastState{};
        };

        void Card(const char* id, const char* label, const std::string& value, float width, ImU32 color = theme::Text) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::Surface);
            ImGui::BeginChild(id, {width, theme::Px(66)}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
            ImGui::SetCursorPos({theme::Px(12), theme::Px(8)});
            {
                w::FontScope caption(theme::CurrentFonts().body, theme::SmallSize);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::Muted);
                ImGui::TextUnformatted(label);
                ImGui::PopStyleColor();
            }
            ImGui::SetCursorPosX(theme::Px(12));
            {
                w::FontScope bold(theme::CurrentFonts().bold, theme::HeadingSize);
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(value.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        std::string CoreValue(const std::vector<Row>& rows, std::string_view key) {
            const auto* row = Find(rows, key);
            return row && row->hasValue ? FormatValue(row->current, row->unit) : "n/a";
        }

        // `values` gives the caption's statistics, `series` the trace.
        void Graph(const char* id, const std::vector<float>& values, const std::vector<float>& series, const char* caption,
                   float height) {
            if (values.empty() || series.empty()) {
                w::Muted(std::format("{}: no samples yet", caption));
                return;
            }
            const auto stats = Compute(values);
            const auto overlay = std::format("{}  avg {:.2f}  p99 {:.2f}  max {:.2f}", caption, stats.average, stats.p99, stats.maximum);
            const float peak = *std::ranges::max_element(series);
            ImGui::PushStyleColor(ImGuiCol_PlotLines, common::HudColor());
            ImGui::PlotLines(id, series.data(), static_cast<int>(series.size()), 0, overlay.c_str(), 0.0F, GraphTop(peak, 16.7F),
                             {ImGui::GetContentRegionAvail().x, height});
            ImGui::PopStyleColor();
        }

        void LiveTab(const B21UI_HostApi* h, const std::vector<Row>& rows) {
            const auto* frame = Find(rows, kFrameMs);
            const auto frames = History(h, frame ? frame->id : 0);
            const auto gpuRow = Find(rows, kGpuMs);
            const auto gpu = History(h, gpuRow ? gpuRow->id : 0);
            const auto recent = std::span<const float>(frames).last(std::min<std::size_t>(frames.size(), 60));
            double sum = 0.0;
            for (const auto v : recent) sum += v;
            const auto lows = FrameLows(frames);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float cardWidth = std::floor((ImGui::GetContentRegionAvail().x - 3 * spacing) / 4);
            ImGui::Dummy({0, theme::Px(4)});
            Card("##fps", "FPS", recent.empty() ? "n/a" : std::format("{:.0f}", 1000.0 * static_cast<double>(recent.size()) / sum),
                 cardWidth, common::HudColor());
            ImGui::SameLine();
            Card("##frame", "Frame", CoreValue(rows, kFrameMs), cardWidth);
            ImGui::SameLine();
            Card("##low1", "1% low", frames.empty() ? "n/a" : std::format("{:.1f} FPS", lows.low1Fps), cardWidth);
            ImGui::SameLine();
            Card("##low01", "0.1% low", frames.empty() ? "n/a" : std::format("{:.1f} FPS", lows.low01Fps), cardWidth);
            Card("##gpu", "GPU frame", CoreValue(rows, kGpuMs), cardWidth);
            ImGui::SameLine();
            Card("##cpu", "Process CPU", CoreValue(rows, kCpuPercent), cardWidth);
            ImGui::SameLine();
            Card("##ram", "Working set", CoreValue(rows, kRamMb), cardWidth);
            ImGui::SameLine();
            Card("##vram", "VRAM", CoreValue(rows, kVramMb), cardWidth);

            auto settings = GetSettings(h);
            w::Heading("Overlay");
            static constexpr std::array modes{"Off", "Compact", "Expanded", "Full"};
            if (const int clicked = w::Segmented("##perfMode", modes, static_cast<int>(settings.mode)); clicked >= 0) {
                settings.mode = static_cast<std::uint32_t>(clicked);
                h->perfSetSettings(&settings);
            }
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            w::Muted("Shift+F11 cycles the overlay, Shift+F12 starts or stops a capture");
            const float labelWidth = theme::Px(90);
            const auto slider = [&](const char* label, const char* id, float& value) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(label);
                ImGui::SameLine(labelWidth);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (w::SliderFloat(id, value, 0.0F, 1.0F, "%.2f")) h->perfSetSettings(&settings);
            };
            slider("Horizontal", "##anchorX", settings.anchorX);
            slider("Vertical", "##anchorY", settings.anchorY);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Full width");
            ImGui::SameLine(labelWidth);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (w::SliderFloat("##overlayWidth", settings.overlayWidth, 0.15F, 1.0F, "%.2f")) h->perfSetSettings(&settings);
            ImGui::SetItemTooltip("Width of the Full overlay as a share of the screen; its metrics fill as many columns as fit.");
            slider("Opacity", "##overlayOpacity", settings.overlayOpacity);

            w::Heading("Frame time (ms)");
            Graph("##frameGraph", frames, GraphSeries(h, frame ? frame->id : 0, frames), "frame", theme::Px(140));
            Graph("##gpuGraph", gpu, GraphSeries(h, gpuRow ? gpuRow->id : 0, gpu), "GPU", theme::Px(100));
        }

        ImU32 DeltaColor(const Row& row, double delta) {
            if (row.baseline == 0.0 || std::abs(delta / row.baseline) < 0.02) return theme::Muted;
            const bool worse = row.unit == B21UI_UNIT_FPS ? delta < 0 : delta > 0;
            return worse ? theme::Error : theme::Ok;
        }

        void MetricsTab(const B21UI_HostApi* h, const std::vector<Row>& rows, PageState& state) {
            ImGui::Dummy({0, theme::Px(4)});
            w::Search("##perfSearch", state.search, sizeof(state.search), "Filter metrics", theme::Px(320));
            const auto query = Lower(state.search);
            const auto matches = [&](const Row& row) {
                return query.empty() || Lower(row.key).contains(query) || Lower(row.label).contains(query) ||
                       Lower(row.group).contains(query);
            };
            std::vector<std::string> owners;
            for (const auto& row : rows)
                if (matches(row) && std::ranges::find(owners, row.owner) == owners.end()) owners.push_back(row.owner);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            w::Muted(std::format("{} metrics from {} owners", rows.size(), owners.size()));
            if (owners.empty()) {
                w::EmptyState(rows.empty() ? "No metrics registered yet." : "No metric matches the filter.");
                return;
            }
            ImGui::BeginChild("##metricList", {0, ListHeight()});
            for (const auto& owner : owners) {
                std::vector<const Row*> items;
                for (const auto& row : rows)
                    if (row.owner == owner && matches(row)) items.push_back(&row);
                std::ranges::stable_sort(items, {}, &Row::group);
                w::Heading(owner, static_cast<int>(items.size()));
                constexpr auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
                if (!ImGui::BeginTable(owner.c_str(), 6, flags)) continue;
                ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthStretch, 3.0F);
                ImGui::TableSetupColumn("Current", ImGuiTableColumnFlags_WidthStretch, 1.2F);
                ImGui::TableSetupColumn("Average", ImGuiTableColumnFlags_WidthStretch, 1.2F);
                ImGui::TableSetupColumn("P99", ImGuiTableColumnFlags_WidthStretch, 1.2F);
                ImGui::TableSetupColumn("vs baseline", ImGuiTableColumnFlags_WidthStretch, 1.4F);
                ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, theme::Px(40));
                ImGui::TableHeadersRow();
                std::string group = "\x01";
                for (const auto* row : items) {
                    if (row->group != group) {
                        group = row->group;
                        if (!group.empty()) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::PushStyleColor(ImGuiCol_Text, theme::Faint);
                            ImGui::TextUnformatted(group.c_str());
                            ImGui::PopStyleColor();
                        }
                    }
                    ImGui::PushID(static_cast<int>(row->id));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(row->label.c_str());
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", row->key.c_str());
                    ImGui::TableSetColumnIndex(1);
                    if (row->unit == B21UI_UNIT_TEXT) {
                        w::Mono(row->text.empty() ? "-" : row->text);
                    } else if (row->hasValue) {
                        w::Mono(FormatValue(row->current, row->unit));
                        ImGui::TableSetColumnIndex(2);
                        w::Mono(FormatValue(row->average, row->unit));
                        ImGui::TableSetColumnIndex(3);
                        w::Mono(FormatValue(row->p99, row->unit));
                        ImGui::TableSetColumnIndex(4);
                        if (row->hasBaseline) {
                            const double delta = row->average - row->baseline;
                            const auto percent = row->baseline != 0.0 ? std::format(" ({:+.0f}%)", delta * 100.0 / row->baseline) : "";
                            w::Mono(std::format("{:+.3g}{}", delta, percent), DeltaColor(*row, delta));
                        } else {
                            w::Mono("-", theme::Faint);
                        }
                    } else {
                        w::Mono("-", theme::Faint);
                    }
                    ImGui::TableSetColumnIndex(5);
                    bool pinned = row->pinned;
                    if (ImGui::Checkbox("##pin", &pinned)) h->perfSetPinned(row->id, pinned ? 1u : 0u);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show on the Expanded overlay");
                    ImGui::PopID();
                }
                ImGui::EndTable();
                ImGui::Dummy({0, theme::Px(8)});
            }
            ImGui::EndChild();
        }

        const char* StateName(std::uint32_t state) {
            switch (state) {
            case B21UI_CAPTURE_WAITING: return "Waiting for the delay";
            case B21UI_CAPTURE_RECORDING: return "Recording";
            case B21UI_CAPTURE_WRITING: return "Writing files";
            default: return "Idle";
            }
        }

        void RefreshFiles(PageState& state) {
            state.files.clear();
            std::error_code error;
            for (const auto& entry : std::filesystem::directory_iterator(CaptureDirectory(), error)) {
                const auto& path = entry.path();
                if (path.extension() == ".json" && path.stem().string().starts_with("B21UI_Capture_"))
                    state.files.push_back({path, path.stem().string()});
            }
            std::ranges::sort(state.files, std::greater{}, &CaptureFile::name);
            if (state.files.size() > 50) state.files.resize(50);
        }

        void CaptureTab(const B21UI_HostApi* h, PageState& state) {
            B21UI_CaptureStatus status{};
            status.size = sizeof(status);
            h->perfCaptureStatus(&status);
            const auto str = [](const char* s) { return std::string(s ? s : ""); };
            const auto csv = str(status.lastCsvPath), json = str(status.lastJsonPath), baseline = str(status.baselinePath);
            const double now = ImGui::GetTime();
            if (now - state.filesAt > 2.0 || status.state != state.lastState) {
                RefreshFiles(state);
                state.filesAt = now;
                state.lastState = status.state;
            }

            auto settings = GetSettings(h);
            ImGui::Dummy({0, theme::Px(4)});
            const float labelWidth = theme::Px(110);
            const auto slider = [&](const char* label, const char* id, float& value, float max, const char* format) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(label);
                ImGui::SameLine(labelWidth);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (w::SliderFloat(id, value, 0.0F, max, format)) h->perfSetSettings(&settings);
            };
            const bool busy = status.state != B21UI_CAPTURE_IDLE;
            ImGui::BeginDisabled(busy);
            slider("Duration", "##captureSeconds", settings.captureSeconds, 300.0F, settings.captureSeconds <= 0.0F ? "until stopped" : "%.0f s");
            slider("Delay", "##captureDelay", settings.captureDelaySeconds, 30.0F, "%.0f s");
            ImGui::EndDisabled();

            if (status.state == B21UI_CAPTURE_IDLE) {
                if (w::Primary("Start capture")) h->perfCaptureStart();
            } else if (status.state == B21UI_CAPTURE_WRITING) {
                w::Button("Writing...", false);
            } else if (w::Danger(status.state == B21UI_CAPTURE_WAITING ? "Cancel" : "Stop capture")) {
                h->perfCaptureStop();
            }
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            const auto stateText = status.state == B21UI_CAPTURE_RECORDING
                ? std::format("{}  {:.1f} s  {} frames", StateName(status.state), status.elapsedSeconds, status.frames)
                : std::string(StateName(status.state));
            w::Mono(stateText, status.state == B21UI_CAPTURE_RECORDING ? theme::Error : theme::Muted);

            if (w::BeginProperties("##captureFiles")) {
                const auto name = [](const std::string& path, const char* none) {
                    return path.empty() ? std::string(none) : ToUtf8(FromUtf8(path).filename());
                };
                w::Property("Last CSV", name(csv, "-"), true);
                w::Property("Last summary", name(json, "-"), true);
                w::Property("Baseline", name(baseline, "none"), true);
                w::Property("Folder", ToUtf8(CaptureDirectory()), true);
                w::EndProperties();
            }
            if (w::Button("Open captures folder")) {
                const auto dir = CaptureDirectory();
                std::error_code error;
                std::filesystem::create_directories(dir, error);
                ::ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            ImGui::SameLine();
            if (w::Button("Clear baseline", !baseline.empty())) h->perfClearBaseline();

            w::Heading("Past captures", static_cast<int>(state.files.size()));
            if (state.files.empty()) {
                w::Muted(std::format("No captures in {} yet.", ToUtf8(CaptureDirectory())));
                return;
            }
            ImGui::BeginChild("##captureList", {0, ListHeight()});
            for (const auto& file : state.files) {
                ImGui::PushID(file.name.c_str());
                const bool current = !baseline.empty() && FromUtf8(baseline) == file.json;
                ImGui::AlignTextToFramePadding();
                w::Mono(file.name, current ? theme::Accent : theme::Text);
                ImGui::SameLine(std::max(ImGui::GetContentRegionAvail().x - theme::Px(160), theme::Px(320)));
                if (current) w::Badge("Baseline", w::Tone::Accent);
                else if (w::Button("Load as baseline")) h->perfLoadBaseline(ToUtf8(file.json).c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
    }

    Metric::Metric(const char* id, const char* label, Unit unit, MetricOptions options) {
        if (!id || !ValidId(id)) return;
        auto& registry = Locals();
        Local* local{};
        {
            std::scoped_lock guard(registry.mutex);
            const auto count = registry.count.load();
            for (std::uint32_t i = 0; i < count; ++i)
                if (registry.items[i]->id == id) {
                    slot_ = i + 1;
                    return;
                }
            if (count >= kMaxLocal) return;
            auto item = std::make_unique<Local>();
            item->id = id;
            item->label = label ? label : id;
            item->group = options.group ? options.group : "";
            item->unit = static_cast<std::uint32_t>(unit);
            item->flags = options.headline ? static_cast<std::uint32_t>(B21UI_METRIC_HEADLINE) : 0u;
            local = item.get();
            registry.items[count] = std::move(item);
            registry.count.store(count + 1, std::memory_order_release);
            slot_ = count + 1;
        }
        Resolve(*local);
    }

    void Metric::Push(double value) const {
        const auto* h = Api();
        if (!B21UI_HOST_HAS(h, perfPush)) return;
        if (auto* local = At(slot_))
            if (const auto handle = Resolve(*local)) h->perfPush(handle, value);
    }

    void Metric::PushText(std::string_view text) const {
        const auto* h = Api();
        if (!B21UI_HOST_HAS(h, perfPushText)) return;
        if (auto* local = At(slot_))
            if (const auto handle = Resolve(*local)) h->perfPushText(handle, std::string(text).c_str());
    }

    bool Active() {
        const auto* h = Api();
        return B21UI_HOST_HAS(h, perfActive) && h->perfActive() != 0;
    }

    bool Capturing() {
        const auto* h = Api();
        return B21UI_HOST_HAS(h, perfActive) && (h->perfActive() & B21UI_PERF_CAPTURE) != 0;
    }

    Mode CurrentMode() {
        const auto* h = Api();
        return B21UI_HOST_HAS(h, perfGetSettings) ? static_cast<Mode>(GetSettings(h).mode) : Mode::Off;
    }

    void SetMode(Mode mode) {
        const auto* h = Api();
        if (!B21UI_HOST_HAS(h, perfSetSettings) || !h->perfGetSettings) return;
        auto settings = GetSettings(h);
        settings.mode = static_cast<std::uint32_t>(mode);
        h->perfSetSettings(&settings);
    }

    bool StartCapture() {
        const auto* h = Api();
        return B21UI_HOST_HAS(h, perfCaptureStart) && h->perfCaptureStart() != 0;
    }

    void StopCapture() {
        const auto* h = Api();
        if (B21UI_HOST_HAS(h, perfCaptureStop)) h->perfCaptureStop();
    }

    void DrawPage(std::span<const PageTab> extraTabs) {
        const auto* h = Api();
        if (!B21UI_HOST_HAS(h, perfClearBaseline)) {
            w::Muted("Performance needs B21UI 14+: the loaded B21UI host is older.");
            return;
        }
        h->perfPageVisible();
        auto& state = common::ContextSlot<PageState>();
        const auto rows = ReadRows(h);
        if (!ImGui::BeginTabBar("##b21uiPerformanceTabs")) return;
        if (ImGui::BeginTabItem("Live")) {
            LiveTab(h, rows);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Metrics")) {
            MetricsTab(h, rows, state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Capture")) {
            CaptureTab(h, state);
            ImGui::EndTabItem();
        }
        for (const auto& tab : extraTabs) {
            if (!tab.label || !ImGui::BeginTabItem(tab.label)) continue;
            if (tab.draw) tab.draw();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

namespace b21ui::client {
    void SetPerfHostApi(const B21UI_HostApi* host) {
        perf::hostApi.store(host, std::memory_order_release);
        auto& registry = perf::Locals();
        const auto count = registry.count.load(std::memory_order_acquire);
        for (std::uint32_t i = 0; i < count; ++i) perf::Resolve(*registry.items[i]);
    }
}
