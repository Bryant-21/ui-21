#include "game/Rendezvous.h"
#include "core/Election.h"
#include "core/Version.h"

#include <Windows.h>
#include <Psapi.h>

#include <spdlog/spdlog.h>

#include <string>
#include <vector>

extern "C" __declspec(dllexport) const B21UI_Rendezvous* B21UI_Rendezvous_v1() {
    static const B21UI_Rendezvous table{sizeof(B21UI_Rendezvous), b21ui::kFrameworkVersion, B21UI_ABI_VERSION,
                                        &b21ui::game::HostApiTable};
    return &table;
}

namespace b21ui::game {
    namespace {
        struct Found {
            HMODULE module{};
            const B21UI_Rendezvous* table{};
        };
        const B21UI_HostApi* elected{};
        bool electedSelf{};
        bool done{};

        std::string ModuleName(HMODULE module) {
            char buffer[MAX_PATH]{};
            const auto length = ::GetModuleFileNameA(module, buffer, MAX_PATH);
            std::string path(buffer, length);
            const auto slash = path.find_last_of("\\/");
            return slash == std::string::npos ? path : path.substr(slash + 1);
        }
    }

    const B21UI_HostApi* Rendezvous() {
        if (done) return elected;
        done = true;
        std::vector<HMODULE> modules(1024);
        DWORD needed{};
        if (!::EnumProcessModules(::GetCurrentProcess(), modules.data(), static_cast<DWORD>(modules.size() * sizeof(HMODULE)), &needed))
            return nullptr;
        modules.resize(std::min<std::size_t>(modules.size(), needed / sizeof(HMODULE)));
        std::vector<Found> found;
        std::vector<core::Candidate> candidates;
        for (auto module : modules) {
            auto fn = reinterpret_cast<B21UI_RendezvousFn>(::GetProcAddress(module, B21UI_RENDEZVOUS_EXPORT));
            if (!fn) continue;
            const auto* table = fn();
            if (!table) continue;
            found.push_back({module, table});
            candidates.push_back({table->frameworkVersion, table->abiVersion, table->size, ModuleName(module)});
        }
        const auto winner = core::Elect(candidates);
        if (!winner) { spdlog::warn("B21UI: no valid host candidate"); return nullptr; }
        const auto* rendezvous = found[*winner].table;
        elected = rendezvous->hostApi ? rendezvous->hostApi() : nullptr;
        if (elected && (elected->size < B21UI_HOSTAPI_MIN_SIZE || elected->abiVersion < 1)) elected = nullptr;
        HMODULE self{};
        ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCWSTR>(&Rendezvous), &self);
        electedSelf = found[*winner].module == self;
        spdlog::info("B21UI: {} candidate(s); host = {} v{}{}", candidates.size(), candidates[*winner].moduleName,
                     candidates[*winner].frameworkVersion, electedSelf ? " (this module)" : "");
        return elected;
    }

    bool IsHost() {
        Rendezvous();
        return electedSelf;
    }
}
