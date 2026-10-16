#include "perf/Overlay.h"
#include "perf/PerfHost.h"
#include "b21ui/Common.h"
#include "b21ui/Paths.h"
#include "b21ui/Perf.h"
#include "b21ui/modern/Theme.h"
#include "b21ui/modern/Widgets.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <initializer_list>
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


        struct Trace {
            std::span<const float> values;
            ImU32 color;
        };

        struct SeriesStats {
            float high{}, low{}, average{};
        };

        // One triangular pass (1 2 3 2 1): neighbouring bins differ by whole frames, so the raw trace is jagged.
        std::vector<float> Smooth(std::span<const float> values) {
            constexpr std::array<float, 5> kernel{1.0F, 2.0F, 3.0F, 2.0F, 1.0F};
            std::vector<float> out(values.size());
            for (std::size_t i = 0; i < values.size(); ++i) {
                float sum = 0.0F, weight = 0.0F;
                for (std::size_t k = 0; k < kernel.size(); ++k) {
                    const auto j = static_cast<std::ptrdiff_t>(i + k) - 2;
                    if (j < 0 || j >= static_cast<std::ptrdiff_t>(values.size())) continue;
                    sum += kernel[k] * values[static_cast<std::size_t>(j)];
                    weight += kernel[k];
                }
                out[i] = sum / weight;
            }
            return out;
        }

        SeriesStats StatsOf(std::span<const float> values) {
            if (values.empty()) return {};
            const auto [low, high] = std::ranges::minmax_element(values);
            double sum = 0.0;
            for (const auto v : values) sum += v;
            return {*high, *low, static_cast<float>(sum / static_cast<double>(values.size()))};
        }

        // A plot box with a labelled value axis (top, middle, 0) and time axis (window ... now), faint grid,
        // smoothed traces, and a dot where the first trace peaks and bottoms out.
        void DrawGraph(const char* id, std::initializer_list<Trace> traces, float top, float windowSeconds, float width, float height) {
            auto* list = ImGui::GetWindowDrawList();
            const float textHeight = ImGui::GetTextLineHeight();
            const float gutter = ImGui::CalcTextSize("0000").x + theme::Px(6);
            const float axisHeight = textHeight + theme::Px(2);
            const auto origin = ImGui::GetCursorScreenPos();
            ImGui::PushID(id);
            ImGui::Dummy({width, height});
            ImGui::PopID();
            const ImVec2 min{origin.x + gutter, origin.y + textHeight * 0.5F};
            const ImVec2 max{origin.x + width, origin.y + height - axisHeight};
            list->AddRectFilled(min, max, IM_COL32(0, 0, 0, 90));
            const ImU32 grid = IM_COL32(255, 255, 255, 28);
            for (int i = 0; i <= 4; ++i) {
                const float y = max.y + (min.y - max.y) * static_cast<float>(i) / 4.0F;
                list->AddLine({min.x, y}, {max.x, y}, grid);
            }
            for (int i = 1; i < 5; ++i) {
                const float x = min.x + (max.x - min.x) * static_cast<float>(i) / 5.0F;
                list->AddLine({x, min.y}, {x, max.y}, grid);
            }
            const auto axis = [&](float value, float y) {
                const auto text = std::format("{:.0f}", value);
                list->AddText({origin.x + gutter - theme::Px(4) - ImGui::CalcTextSize(text.c_str()).x, y - textHeight * 0.5F}, theme::Muted,
                              text.c_str());
            };
            axis(top, min.y);
            axis(top * 0.5F, (min.y + max.y) * 0.5F);
            axis(0.0F, max.y);
            const auto windowText = std::format("{:.0f}s", windowSeconds);
            list->AddText({min.x, max.y + theme::Px(1)}, theme::Muted, windowText.c_str());
            list->AddText({max.x - ImGui::CalcTextSize("0").x, max.y + theme::Px(1)}, theme::Muted, "0");

            list->PushClipRect(min, max, true);
            bool first = true;
            std::vector<ImVec2> points;
            for (const auto& trace : traces) {
                if (trace.values.size() < 2) continue;
                const auto smooth = Smooth(trace.values);
                points.clear();
                for (std::size_t i = 0; i < smooth.size(); ++i)
                    points.push_back({min.x + (max.x - min.x) * static_cast<float>(i) / static_cast<float>(smooth.size() - 1),
                                      max.y - (max.y - min.y) * std::clamp(smooth[i] / top, 0.0F, 1.0F)});
                if (first)
                    for (std::size_t i = 0; i + 1 < points.size(); ++i)
                        list->AddQuadFilled(points[i], points[i + 1], {points[i + 1].x, max.y}, {points[i].x, max.y}, Fade(trace.color, 0.14F));
                list->AddPolyline(points.data(), static_cast<int>(points.size()), trace.color, ImDrawFlags_None, theme::Px(1.6F));
                if (first) {
                    const auto [low, high] = std::ranges::minmax_element(smooth);
                    list->AddCircleFilled(points[static_cast<std::size_t>(high - smooth.begin())], theme::Px(3), trace.color);
                    list->AddCircleFilled(points[static_cast<std::size_t>(low - smooth.begin())], theme::Px(3), trace.color);
                }
                first = false;
            }
            list->PopClipRect();
            list->AddRect(min, max, IM_COL32(255, 255, 255, 40));
        }

        // A colour swatch, a name and a right-aligned value, the way PresentMon lists its series.
        void DrawLegendRow(ImU32 color, const char* label, const std::string& value, float valueX) {
            const float side = ImGui::GetTextLineHeight() * 0.7F;
            const auto pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled({pos.x, pos.y + (ImGui::GetTextLineHeight() - side) * 0.5F},
                                                      {pos.x + side, pos.y + (ImGui::GetTextLineHeight() + side) * 0.5F}, color);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + side + theme::Px(6));
            ImGui::PushStyleColor(ImGuiCol_Text, theme::Text);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::SetCursorPosX(valueX);
            w::Mono(value, color);
        }

        std::string ReadoutText(const SeriesStats& stats, const char* unit, int decimals) {
            return std::format("High {:.{}f}   Low {:.{}f}   Avg {:.{}f} {}", stats.high, decimals, stats.low, decimals, stats.average,
                               decimals, unit);
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
                    std::vector<float> fps(series.size());
                    for (std::size_t i = 0; i < series.size(); ++i) fps[i] = series[i] > 0.0F ? 1000.0F / series[i] : 0.0F;
                    const auto fpsStats = StatsOf(fps);
                    const auto lows = FrameLows(history_);
                    const float valueX = ImGui::GetCursorPosX() + theme::Px(150);
                    ImGui::Dummy({0, theme::Px(2)});
                    DrawLegendRow(hud, "FPS (avg)", std::format("{:.1f} fps", fpsStats.average), valueX);
                    DrawLegendRow(theme::Error, "FPS (1% low)", std::format("{:.1f} fps", lows.low1Fps), valueX);
                    DrawLegendRow(theme::Warn, "FPS (0.1% low)", std::format("{:.1f} fps", lows.low01Fps), valueX);
                    const float graphWidth = std::max(theme::Px(300), ImGui::GetContentRegionAvail().x);
                    const float fpsTop = std::max(GraphTop(fpsStats.high, 30.0F), 60.0F);
                    DrawGraph("fps", {{fps, hud}}, fpsTop, static_cast<float>(kGraphWindowSeconds), graphWidth, theme::Px(92));
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::Muted);
                    ImGui::TextUnformatted(ReadoutText(fpsStats, "fps", 0).c_str());
                    ImGui::PopStyleColor();

                    gpuTimedValues_.resize(MetricStore::kHistory);
                    gpuTimedTimes_.resize(MetricStore::kHistory);
                    const auto gpuTimed = store.HistoryTimed(core.gpuMs, gpuTimedValues_.data(), gpuTimedTimes_.data(), MetricStore::kHistory);
                    const auto gpuSeries = BinByTime(std::span<const float>(gpuTimedValues_).first(gpuTimed),
                                                     std::span<const double>(gpuTimedTimes_).first(gpuTimed), host.Now(),
                                                     kGraphWindowSeconds, kGraphBins / 2);
                    const auto msStats = StatsOf(series);
                    const auto gpuStats = StatsOf(gpuSeries);
                    ImGui::Dummy({0, theme::Px(6)});
                    DrawLegendRow(hud, "Frame time (avg)", std::format("{:.2f} ms", msStats.average), valueX);
                    if (!gpuSeries.empty()) DrawLegendRow(theme::Warn, "GPU busy (avg)", std::format("{:.2f} ms", gpuStats.average), valueX);
                    const float msTop = std::max(GraphTop(std::max(msStats.high, gpuStats.high), 16.7F), 33.4F);
                    DrawGraph("frametime", {{series, hud}, {gpuSeries, theme::Warn}}, msTop, static_cast<float>(kGraphWindowSeconds), graphWidth,
                              theme::Px(92));
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::Muted);
                    ImGui::TextUnformatted(ReadoutText(msStats, "ms", 1).c_str());
                    ImGui::PopStyleColor();
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
            std::vector<float> gpuTimedValues_;
            std::vector<double> gpuTimedTimes_;
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
