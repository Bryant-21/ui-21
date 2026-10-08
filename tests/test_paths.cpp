#include <doctest/doctest.h>
#include "kit/PathsPure.h"

TEST_CASE("plugin asset dir sits next to the DLL, named after it") {
    const auto dir = b21ui::kit::AssetDirForModule("Fallout 4/Data/F4SE/Plugins/B21_FullScreenMap.dll");
    CHECK(dir == std::filesystem::path("Fallout 4/Data/F4SE/Plugins/B21_FullScreenMap"));
}
