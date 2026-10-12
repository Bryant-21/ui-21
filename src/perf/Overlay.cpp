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
            // SameLine's offset is measured from the window edge, not from the content start.
            labelWidth += theme::Px(16) + ImGui::GetCursorPosX();
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
                ImGui::SameLine(labelWidth);
                w::Mono(line.value);
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
                ImGui::SetNextWindowBgAlpha(0.80F);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {theme::Px(12), theme::Px(10)});
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {theme::Px(8), theme::Px(3)});
                constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus;
                if (ImGui::Begin("##b21uiPerformance", nullptr, flags)) {
                    history_.resize(MetricStore::kHistory);
                    history_.resize(store.History(core.frameMs, history_.data(), MetricStore::kHistory));
                    const auto recent = std::span<const float>(history_).last(std::min<std::size_t>(history_.size(), 60));
                    double sum = 0.0;
                    for (const auto v : recent) sum += v;
                    const double average = recent.empty() ? 0.0 : sum / static_cast<double>(recent.size());
                    const double latest = history_.empty() ? 0.0 : history_.back();
                    const double gpu = store.Current(core.gpuMs);
                    {
                        w::FontScope bold(theme::CurrentFonts().bold, theme::HeadingSize);
                        ImGui::PushStyleColor(ImGuiCol_Text, hud);
                        ImGui::Text("%.0f", average > 0.0 ? 1000.0 / average : 0.0);
                        ImGui::PopStyleColor();
                    }
                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::Muted);
                    if (std::isfinite(gpu)) ImGui::Text("FPS \xC2\xB7 %.2f ms \xC2\xB7 GPU %.2f ms", latest, gpu);
                    else ImGui::Text("FPS \xC2\xB7 %.2f ms", latest);
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
                if (!history_.empty()) {
                    const auto shown = std::span<const float>(history_).last(std::min<std::size_t>(history_.size(), 240));
                    const float top = std::max(33.4F, static_cast<float>(Percentile(shown, 0.99)) * 1.25F);
                    ImGui::PushStyleColor(ImGuiCol_PlotLines, hud);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 90));
                    ImGui::PlotLines("##frames", shown.data(), static_cast<int>(shown.size()), 0, nullptr, 0.0F, top,
                                     {theme::Px(260), theme::Px(56)});
                    ImGui::PopStyleColor(2);
                }
                std::vector<Line> lines;
                const auto lows = FrameLows(history_);
                lines.push_back({"1% low", history_.empty() ? "n/a" : std::format("{:.1f} FPS", lows.low1Fps)});
                lines.push_back({"0.1% low", history_.empty() ? "n/a" : std::format("{:.1f} FPS", lows.low01Fps)});
                const auto coreLine = [&](const char* label, std::uint32_t handle, std::uint32_t unit) {
                    const double value = store.Current(handle);
                    lines.push_back({label, std::isfinite(value) ? FormatValue(value, unit) : "n/a"});
                };
                coreLine("GPU", core.gpuMs, B21UI_UNIT_MS);
                coreLine("CPU", core.cpu, B21UI_UNIT_PERCENT);
                coreLine("RAM", core.ram, B21UI_UNIT_MB);
                coreLine("VRAM", core.vram, B21UI_UNIT_MB);

                const auto count = store.Count();
                std::vector<std::string> owners;
                for (std::uint32_t i = 1; i <= count; ++i) {
                    const auto* meta = store.Meta(i);
                    if (meta->owner != "b21ui" && std::ranges::find(owners, meta->owner) == owners.end()) owners.push_back(meta->owner);
                }
                MetricStore::Snapshot snapshot;
                for (const auto& owner : owners) {
                    if (mode >= B21UI_PERF_MODE_FULL) break;
                    bool header = false;
                    for (std::uint32_t i = 1; i <= count; ++i) {
                        const auto* meta = store.Meta(i);
                        if (meta->owner != owner || !host.IsPinned(i)) continue;
                        if (!header) {
                            lines.push_back({Upper(owner), {}, true});
                            header = true;
                        }
                        store.Read(i, snapshot);
                        lines.push_back({meta->label, ValueText(*meta, snapshot, false)});
                    }
                }
                DrawLines(lines, hud);
                if (mode < B21UI_PERF_MODE_FULL) return;

                lines.clear();
                for (const auto& owner : owners) {
                    lines.push_back({Upper(owner), {}, true});
                    for (std::uint32_t i = 1; i <= count; ++i) {
                        const auto* meta = store.Meta(i);
                        if (meta->owner != owner) continue;
                        store.Read(i, snapshot);
                        lines.push_back({meta->label, ValueText(*meta, snapshot, true)});
                    }
                }
                ImGui::Dummy({0, theme::Px(2)});
                ImGui::Separator();
                DrawLines(lines, hud);
            }

            std::vector<float> history_;
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
                               .settingsIcon = modern::icon::Performance, .settingsCategory = "B21UI"});
    }
}
