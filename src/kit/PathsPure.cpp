#include "kit/PathsPure.h"

namespace b21ui::kit {
    std::filesystem::path AssetDirForModule(const std::filesystem::path& modulePath) {
        return modulePath.parent_path() / modulePath.stem();
    }
}
