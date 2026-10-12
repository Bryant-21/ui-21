#pragma once
#include <array>
#include <cstdint>
#include <optional>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Query;
struct IDXGIAdapter3;

namespace b21ui::perf {
    // One timestamp pair (+ disjoint) per frame, read back a few frames later without stalling.
    class GpuTimer {
    public:
        ~GpuTimer() { Release(); }
        // Call once per Present; returns the newest finished GPU frame time.
        std::optional<double> Frame(ID3D11Device* device, ID3D11DeviceContext* context);
        // Closes an open measurement without keeping it (the sampler is going idle).
        void Abandon(ID3D11DeviceContext* context);
        void Release();

    private:
        struct Set {
            ID3D11Query* disjoint{};
            ID3D11Query* begin{};
            ID3D11Query* end{};
            bool pending{};
            bool discard{};
            std::uint64_t order{};
        };
        bool Create(ID3D11Device* device);
        std::array<Set, 5> sets_{};
        ID3D11Device* device_{};
        int open_ = -1;
        std::uint64_t issued_{};
        bool failed_{};
    };

    // Process CPU %, working set and local VRAM usage.
    class ProcessSampler {
    public:
        ~ProcessSampler() { Release(); }
        struct Sample {
            std::optional<double> cpuPercent, ramMb, vramMb;
        };
        Sample Read(ID3D11Device* device);
        void Release();

    private:
        std::uint64_t lastWall{}, lastCpu{};
        IDXGIAdapter3* adapter_{};
        ID3D11Device* adapterDevice_{};
    };
}
