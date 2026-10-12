#pragma once
#include "b21ui/Abi.h"
#include "perf/CaptureWriter.h"
#include "perf/GpuSampler.h"
#include "perf/MetricStore.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <mutex>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;

// The elected host's performance service: metric store, core sampler, settings, capture. No game code,
// so the desktop preview runs it too.
namespace b21ui::perf {
    // Documents\My Games\Fallout4\F4SE\B21UI\Captures
    std::filesystem::path CaptureDirectory();
    // Fills the framework 14 fields of a host table.
    void FillHostApi(B21UI_HostApi& table);

    class Host {
    public:
        static Host& Get();

        // Registers the core metrics, the overlay and the MOD SETTINGS page. `info` lands in every
        // capture summary (game runtime, loaded B21UI mods).
        void Start(nlohmann::json info = nlohmann::json::object());
        // Before Start: keep settings in another ini (the preview).
        void SetSettingsFile(std::filesystem::path file) { settingsFile_ = std::move(file); }
        // Render thread, once per Present, before any UI draws.
        void OnPresent(ID3D11Device* device, ID3D11DeviceContext* context);
        void DeviceLost();

        struct Core {
            std::uint32_t frameMs{}, fps{}, gpuMs{}, cpu{}, ram{}, vram{};
        };
        MetricStore& Store() { return store_; }
        const Core& CoreIds() const { return core_; }
        std::uint32_t Active() const;
        std::uint32_t Mode() const { return mode_.load(); }
        B21UI_PerfSettings Settings() const;
        void SetSettings(const B21UI_PerfSettings& settings);
        void SetMode(std::uint32_t mode);
        void CycleMode();
        bool IsPinned(std::uint32_t handle) const;
        double Now() const;
        void SetPinned(std::uint32_t handle, bool pinned);
        void PageVisible();

        bool StartCapture();
        void StopCapture();
        void ToggleCapture();
        struct CaptureView {
            std::uint32_t state{};
            float elapsedSeconds{};
            std::uint32_t frames{};
            std::string csv, json, baseline;
        };
        CaptureView Capture() const;
        bool LoadBaseline(const std::filesystem::path& file);
        void ClearBaseline();
        std::optional<double> Baseline(const std::string& id) const;

    private:
        void LoadSettings();
        void SaveSettings();
        void ApplyOverlay();
        void FinishCapture();

        MetricStore store_;
        Core core_;
        GpuTimer gpu_;
        ProcessSampler process_;
        bool started_{};
        bool sampling_{};
        std::int64_t lastPresent_{};
        double lastProcessSample_ = -1e9;

        mutable std::mutex mutex_;  // settings, pins, baseline, capture paths
        std::filesystem::path settingsFile_;
        B21UI_PerfSettings settings_{sizeof(B21UI_PerfSettings), B21UI_PERF_MODE_OFF, 1.0F, 0.0F, 30.0F, 0.0F, 0.5F, 0.8F};
        PinOverrides pins_;
        std::atomic<std::uint32_t> mode_{B21UI_PERF_MODE_OFF};
        bool settingsDirty_{};
        double settingsChangedAt_{};
        std::atomic<double> pageSeenAt_{-1e9};
        std::map<std::string, double> baseline_;
        std::string baselinePath_;
        nlohmann::json info_ = nlohmann::json::object();

        std::atomic<std::uint32_t> captureState_{B21UI_CAPTURE_IDLE};
        std::atomic<bool> stopRequested_{};
        double captureArmedAt_{}, captureStartedAt_{};
        float captureSeconds_{}, captureDelay_{};
        std::atomic<float> captureElapsed_{};
        std::atomic<std::uint32_t> captureFrames_{};
        CaptureData capture_;
        FrameRow row_;
        std::string lastCsv_, lastJson_;
    };
}
