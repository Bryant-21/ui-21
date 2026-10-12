#include "perf/GpuSampler.h"

#include <Windows.h>
#include <Psapi.h>
#include <d3d11.h>
#include <dxgi1_4.h>

#include <algorithm>

namespace b21ui::perf {
    namespace {
        template <class T>
        void SafeRelease(T*& p) {
            if (p) p->Release();
            p = nullptr;
        }
        std::uint64_t FileTimeTicks(const FILETIME& t) {
            return (static_cast<std::uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
        }
    }

    bool GpuTimer::Create(ID3D11Device* device) {
        Release();
        device_ = device;
        D3D11_QUERY_DESC disjoint{D3D11_QUERY_TIMESTAMP_DISJOINT, 0}, stamp{D3D11_QUERY_TIMESTAMP, 0};
        for (auto& set : sets_)
            if (FAILED(device->CreateQuery(&disjoint, &set.disjoint)) || FAILED(device->CreateQuery(&stamp, &set.begin)) ||
                FAILED(device->CreateQuery(&stamp, &set.end))) {
                Release();
                device_ = device;
                failed_ = true;
                return false;
            }
        return true;
    }

    void GpuTimer::Release() {
        for (auto& set : sets_) {
            SafeRelease(set.disjoint);
            SafeRelease(set.begin);
            SafeRelease(set.end);
            set.pending = set.discard = false;
        }
        device_ = nullptr;
        open_ = -1;
        failed_ = false;
    }

    void GpuTimer::Abandon(ID3D11DeviceContext* context) {
        if (open_ < 0 || !context) return;
        auto& set = sets_[open_];
        context->End(set.end);
        context->End(set.disjoint);
        set.pending = true;
        set.discard = true;
        set.order = ++issued_;
        open_ = -1;
    }

    std::optional<double> GpuTimer::Frame(ID3D11Device* device, ID3D11DeviceContext* context) {
        if (!device || !context) return std::nullopt;
        if (device != device_) {
            if (!Create(device)) return std::nullopt;
        }
        if (failed_) return std::nullopt;
        if (open_ >= 0) {
            auto& set = sets_[open_];
            context->End(set.end);
            context->End(set.disjoint);
            set.pending = true;
            set.order = ++issued_;
            open_ = -1;
        }
        std::optional<double> result;
        // Oldest first; stop at the first one the GPU has not finished.
        for (;;) {
            Set* oldest{};
            for (auto& set : sets_)
                if (set.pending && (!oldest || set.order < oldest->order)) oldest = &set;
            if (!oldest) break;
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
            if (context->GetData(oldest->disjoint, &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) break;
            UINT64 begin{}, end{};
            const bool ready = context->GetData(oldest->begin, &begin, sizeof(begin), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
                               context->GetData(oldest->end, &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
            if (!ready) break;
            if (!oldest->discard && !disjoint.Disjoint && disjoint.Frequency > 0 && end >= begin)
                result = static_cast<double>(end - begin) * 1000.0 / static_cast<double>(disjoint.Frequency);
            oldest->pending = oldest->discard = false;
        }
        for (int i = 0; i < static_cast<int>(sets_.size()); ++i) {
            if (sets_[i].pending) continue;
            context->Begin(sets_[i].disjoint);
            context->End(sets_[i].begin);
            open_ = i;
            break;
        }
        return result;
    }

    ProcessSampler::Sample ProcessSampler::Read(ID3D11Device* device) {
        Sample sample;
        FILETIME created{}, exited{}, kernel{}, user{}, now{};
        ::GetSystemTimeAsFileTime(&now);
        if (::GetProcessTimes(::GetCurrentProcess(), &created, &exited, &kernel, &user)) {
            const auto cpu = FileTimeTicks(kernel) + FileTimeTicks(user), wall = FileTimeTicks(now);
            if (lastWall && wall > lastWall) {
                SYSTEM_INFO info{};
                ::GetSystemInfo(&info);
                const auto cores = std::max<DWORD>(1, info.dwNumberOfProcessors);
                sample.cpuPercent = static_cast<double>(cpu - lastCpu) * 100.0 / static_cast<double>(wall - lastWall) / cores;
            }
            lastWall = wall;
            lastCpu = cpu;
        }
        PROCESS_MEMORY_COUNTERS_EX memory{};
        if (::GetProcessMemoryInfo(::GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)))
            sample.ramMb = static_cast<double>(memory.WorkingSetSize) / (1024.0 * 1024.0);
        if (device && device != adapterDevice_) {
            SafeRelease(adapter_);
            adapterDevice_ = device;
            IDXGIDevice* dxgi{};
            if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))) {
                IDXGIAdapter* adapter{};
                if (SUCCEEDED(dxgi->GetAdapter(&adapter))) {
                    adapter->QueryInterface(IID_PPV_ARGS(&adapter_));
                    adapter->Release();
                }
                dxgi->Release();
            }
        }
        if (adapter_) {
            DXGI_QUERY_VIDEO_MEMORY_INFO info{};
            if (SUCCEEDED(adapter_->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
                sample.vramMb = static_cast<double>(info.CurrentUsage) / (1024.0 * 1024.0);
        }
        return sample;
    }

    void ProcessSampler::Release() {
        SafeRelease(adapter_);
        adapterDevice_ = nullptr;
    }
}
