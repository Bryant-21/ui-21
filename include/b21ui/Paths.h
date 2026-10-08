#pragma once
#include <filesystem>

namespace b21ui {
    // Data/F4SE/Plugins/<ThisDllStem>/ — every UI asset of a mod lives here.
    std::filesystem::path PluginAssetDir();
    // Preview exe only: point the kit at a mod's asset folder.
    void SetAssetDirOverride(std::filesystem::path dir);
}
