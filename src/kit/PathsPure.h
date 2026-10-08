#pragma once
#include <filesystem>

namespace b21ui::kit {
    std::filesystem::path AssetDirForModule(const std::filesystem::path& modulePath);
}
