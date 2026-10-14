#include "perf/Overlay.h"
#include "perf/PerfHost.h"
#include "b21ui/Common.h"
#include "b21ui/Paths.h"
#include "b21ui/Perf.h"
#include "b21ui/modern/Theme.h"
#include "b21ui/modern/Widgets.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace b21ui::perf {
    namespace {
        namespace theme = modern::theme;
        namespace w = modern::w;

        struct Line {
            std::string label, value;
            bool header{};
        };

        double RecentAverage(std::span<const float> history) {
            const auto recent = history.last(std::min<std::size_t>(history.size(), 60));
            double sum = 0.0;
            for (const auto v : recent) sum += v;
            return recent.empty() ? 0.0 : sum / static_cast<double>(recent.size());
        }

        ImU32 Fade(ImU32 color, float alpha) {
            return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(std::clamp(alpha, 0.0F, 1.0F) * 255.0F) << IM_COL32_A_SHIFT);
        }

        std::string Upper(std::string text) {
            std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return text;
        }

        std::string ValueText(const MetricMeta& meta, const MetricStore::Snapshot& s, bool withAverage) {
            if (meta.unit == B21UI_UNIT_TEXT) return s.text.empty() ? "-" : s.text;
            if (!s.hasValue) return "-";
            auto text = FormatValue(s.current, meta.unit);
            if (withAverage && s.stats.samples > 1) text += std::format("  ({})", FormatValue(s.stats.average, meta.unit));
            return text;
        }

        void DrawLines(const std::vector<Line>& lines, ImU32 hud) {
            float labelWidth = 0.0F;
            for (const auto& line : lines)
                if (!line.header) labelWidth = std::max(labelWidth, ImGui::CalcTextSize(line.label.c_str()).x);
            // Measured from where the lines start, so a column of the full list lines up too.
            const float startX = ImGui::GetCursorPosX();
            labelWidth += theme::Px(16);
            for (const auto& line : lines) {
                if (line.header) {
                    ImGui::Dummy({0, theme::Px(2)});
                    w::FontScope bold(theme::CurrentFonts().bold, theme::SmallSize - 2.0F);
                    ImGui::PushStyleColor(ImGuiCol_Text, Fade(hud, 0.85F));
                    ImGui::TextUnformatted(line.label.c_str());
                    ImGui::PopStyleColor();
                    continue;
                }
                ImGui::PushStyleColor(ImGuiCol_Text, theme::Muted);
                ImGui::TextUnformatted(line.label.c_str());
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::SetCursorPosX(startX + labelWidth);
                w::Mono(line.value);
            }
        }

        // The full list in as many columns as the overlay width holds, each column a group.
        void DrawLineColumns(const std::vector<Line>& lines, ImU32 hud) {
            float labelWidth = 0.0F, valueWidth = 0.0F;
            for (const auto& line : lines) {
                labelWidth = std::max(labelWidth, ImGui::CalcTextSize(line.label.c_str()).x);
                if (!line.header) valueWidth = std::max(valueWidth, ImGui::CalcTextSize(line.value.c_str()).x);
            }
            const float gap = theme::Px(24);
            const float columnWidth = labelWidth + theme::Px(16) + valueWidth + gap;
            const auto columns = OverlayColumns(ImGui::GetContentRegionAvail().x + gap, columnWidth);
            const auto rows = RowsPerColumn(lines.size(), columns);
            if (columns <= 1 || rows == 0) {
                DrawLines(lines, hud);
                return;
            }
            for (std::size_t begin = 0; begin < lines.size(); begin += rows) {
                if (begin != 0) ImGui::SameLine(0, gap);
                ImGui::BeginGroup();
                DrawLines({lines.begin() + static_cast<std::ptrdiff_t>(begin),
                           lines.begin() + static_cast<std::ptrdiff_t>(std::min(lines.size(), begin + rows))},
                          hud);
                ImGui::EndGroup();
            }
        }

        class OverlayView final : public Client {
        public:
            void OnContextCreated(const FrameContext&) override { theme::LoadFonts(PluginAssetDir() / "fonts"); }

            void Draw(const FrameContext& frame) override {
                auto& host = Host::Get();
                const auto mode = host.Mode();
                if (mode == B21UI_PERF_MODE_OFF) return;
                theme::SetAppearanceClient("UI21_Performance");
                theme::Apply(std::max(1.0F, frame.height / 1080.0F));
                const auto settings = host.Settings();
                const ImU32 hud = common::HudColor();
                auto& store = host.Store();
                const auto& core = host.CoreIds();

                ImGui::PushFont(theme::CurrentFonts().body, theme::Px(theme::SmallSize));
                const float margin = theme::Px(14);
                ImGui::SetNextWindowPos({margin + (frame.width - 2 * margin) * settings.anchorX,
                                         margin + (frame.height - 2 * margin) * settings.anchorY},
                                        ImGuiCond_Always, {settings.anchorX, settings.anchorY});
                ImGui::SetNextWindowBgAlpha(settings.overlayOpacity);
                if (mode >= B21UI_PERF_MODE_FULL) {
                    const float width = (frame.width - 2 * margin) * settings.overlayWidth;
                    ImGui::SetNextWindowSizeConstraints({width, 0}, {width, frame.height - 2 * margin});
                }
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {theme::Px(12), theme::Px(10)});
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {theme::Px(8), theme::Px(3)});
                constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus;
                if (ImGui::Begin("##b21uiPerformance", nullptr, flags)) {
                    history_.resize(MetricStore::kHistory);
                    history_.resize(store.History(core.frameMs, history_.data(), MetricStore::kHistory));
                    const double average = RecentAverage(history_);
                    gpuHistory_.resize(MetricStore::kHistory);
                    gpuHistory_.resize(store.History(core.gpuMs, gpuHistory_.data(), MetricStore::kHistory));
                    const double gpu = gpuHistory_.empty() ? std::numeric_limits<double>::quiet_NaN() : RecentAverage(gpuHistory_);
                    {
                        w::FontScope bold(theme::CurrentFonts().bold, theme::HeadingSize);
                        ImGui::PushStyleColor(ImGuiCol_Text, hud);
                        ImGui::Text("%.0f", average > 0.0 ? 1000.0 / average : 0.0);
                        ImGui::PopStyleColor();
                    }
                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::Muted);
                    // The 60-frame average beside the FPS it explains: one raw frame made the text flicker.
                    if (std::isfinite(gpu)) ImGui::Text("FPS \xC2\xB7 %.2f ms \xC2\xB7 GPU %.2f ms", average, gpu);
                    else ImGui::Text("FPS \xC2\xB7 %.2f ms", average);
                    ImGui::PopStyleColor();
                    if (const auto state = host.Capture().state; state != B21UI_CAPTURE_IDLE) {
                        ImGui::SameLine();
                        ImGui::PushStyleColor(ImGuiCol_Text, state == B21UI_CAPTURE_RECORDING ? theme::Error : theme::Warn);
                        ImGui::TextUnformatted(state == B21UI_CAPTURE_RECORDING ? "\xE2\x97\x8F REC" :
                                               state == B21UI_CAPTURE_WAITING ? "WAIT" : "SAVING");
                        ImGui::PopStyleColor();
                    }
                    if (mode >= B21UI_PERF_MODE_EXPANDED) DrawExpanded(host, mode, hud);
                }
                ImGui::End();
                ImGui::PopStyleVar(2);
                ImGui::PopFont();
            }

        private:
            void DrawExpanded(Host& host, std::uint32_t mode, ImU32 hud) {
                auto& store = host.Store();
                const auto& core = host.CoreIds();
                timedValues_.resize(MetricStore::kHistory);
                timedTimes_.resize(MetricStore::kHistory);
                const auto timed = store.HistoryTimed(core.frameMs, timedValues_.data(), timedTimes_.data(), MetricStore::kHistory);
                const auto series = BinByTime(std::span<const float>(timedValues_).first(timed),
                                              std::span<const double>(timedTimes_).first(timed), host.Now(), kGraphWindowSeconds,
                                              kGraphBins / 2);
                if (!series.empty()) {
                    const float top = GraphTop(*std::ranges::max_element(series), 16.7F);
                    ImGui::PushStyleColor(ImGuiCol_PlotLines, hud);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 90));
                    ImGui::PlotLines("##frames", series.data(), static_cast<int>(series.size()), 0, nullptr, 0.0F,
                                     std::max(top, 33.4F), {std::max(theme::Px(260), ImGui::GetContentRegionAvail().x), theme::Px(56)});
                    ImGui::PopStyleColor(2);
                }
                // Text is rebuilt four times a second: formatting every metric each frame cost more than the
                // overlay draw, and values that change every frame cannot be read anyway.
                const double now = host.Now();
                if (mode != linesMode_ || !(now - linesAt_ < 0.25)) {
                    linesAt_ = now;
                    linesMode_ = mode;
                    pinnedLines_.clear();
                    fullLines_.clear();
                    BuildLines(host, mode);
                }
                DrawLines(pinnedLines_, hud);
                if (mode < B21UI_PERF_MODE_FULL) return;
                ImGui::Dummy({0, theme::Px(2)});
                ImGui::Separator();
                DrawLineColumns(fullLines_, hud);
            }

            void BuildLines(Host& host, std::uint32_t mode) {
                auto& store = host.Store();
                auto& lines = pinnedLines_;
                const auto lows = FrameLows(history_);
                lines.push_back({"1% low", history_.empty() ? "n/a" : std::format("{:.1f} FPS", lows.low1Fps)});
                lines.push_back({"0.1% low", history_.empty() ? "n/a" : std::format("{:.1f} FPS", lows.low01Fps)});

                const auto count = store.Count();
                std::vector<const MetricMeta*> metas;
                std::vector<PinView> views;
                for (std::uint32_t i = 1; i <= count; ++i) {
                    const auto* meta = store.Meta(i);
                    metas.push_back(meta);
                    views.push_back({meta->id, meta->owner, host.IsPinned(i)});
                }
                MetricStore::Snapshot snapshot;
                // Full lists every metric below, so its pinned block keeps only the host's own rows.
                for (const auto& row : PinnedRows(views)) {
                    const auto* meta = metas[row.index];
                    if (mode >= B21UI_PERF_MODE_FULL && meta->owner != "b21ui") break;
                    if (row.header) {
                        lines.push_back({Upper(meta->owner), {}, true});
                        continue;
                    }
                    store.Read(static_cast<std::uint32_t>(row.index + 1), snapshot);
                    lines.push_back({meta->label, ValueText(*meta, snapshot, false)});
                }

                if (mode < B21UI_PERF_MODE_FULL) return;
                std::vector<std::string_view> owners;
                for (const auto* meta : metas)
                    if (meta->owner != "b21ui" && std::ranges::find(owners, meta->owner) == owners.end()) owners.push_back(meta->owner);
                auto& full = fullLines_;
                for (const auto owner : owners) {
                    full.push_back({Upper(std::string(owner)), {}, true});
                    for (std::uint32_t i = 1; i <= count; ++i) {
                        const auto* meta = metas[i - 1];
                        if (meta->owner != owner) continue;
                        store.Read(i, snapshot);
                        full.push_back({meta->label, ValueText(*meta, snapshot, true)});
                    }
                }
            }

            std::vector<float> history_;
            std::vector<float> timedValues_;
            std::vector<double> timedTimes_;
            std::vector<float> gpuHistory_;
            std::vector<Line> pinnedLines_, fullLines_;
            double linesAt_ = -1.0;
            std::uint32_t linesMode_{};
        };

        class PageView final : public Client {
        public:
            void Draw(const FrameContext&) override {
                ImGui::SetCursorPos({theme::Px(24), theme::Px(20)});
                ImGui::BeginChild("performanceContent", {-theme::Px(24), -theme::Px(16)});
                w::Title("Performance");
                b21ui::perf::DrawPage();
                ImGui::EndChild();
            }
        };

        OverlayView overlay;
        PageView page;
    }

    Client& OverlayClient() { return overlay; }

    void RegisterPerfClients() {
        b21ui::Register(overlay, {.name = "UI21_Performance", .modal = false, .padPointer = false});
        b21ui::Register(page, {.name = "UI21_PerformancePage", .settings = true, .settingsLabel = "Performance",
                               .settingsIcon = modern::icon::Performance, .settingsCategory = "B21UI", .settingsVersion = ""});
    }
}
