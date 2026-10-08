#include "b21ui/Paths.h"
#include "kit/PathsPure.h"

#include <Windows.h>

#include <optional>

namespace b21ui {
    namespace {
        std::optional<std::filesystem::path>& Override() {
            static std::optional<std::filesystem::path> dir;
            return dir;
        }
    }

    void SetAssetDirOverride(std::filesystem::path dir) { Override() = std::move(dir); }

    std::filesystem::path PluginAssetDir() {
        if (Override()) return *Override();
        HMODULE module{};
        ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCWSTR>(&PluginAssetDir), &module);
        wchar_t buffer[MAX_PATH]{};
        const auto length = ::GetModuleFileNameW(module, buffer, MAX_PATH);
        return kit::AssetDirForModule(std::filesystem::path(std::wstring(buffer, length)));
    }
}
