#pragma once
#include <filesystem>

namespace b21ui::preview {
    void SetHudColor(float r, float g, float b);
    int Run(int argc, char** argv, const std::filesystem::path& assetDir);
}
