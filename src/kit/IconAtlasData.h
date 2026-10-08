#pragma once
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace b21ui::kit {
    struct IconRect {
        float u0{}, v0{}, u1{}, v1{};
    };

    struct IconEntry {
        bool tintable{};
        std::array<IconRect, 4> bands{};   // shadow, dim, mid, bright
        IconRect shape{};
    };

    struct IconAtlasData {
        std::string texture;
        int width{};
        int height{};
        std::unordered_map<std::string, IconEntry> icons;
    };

    std::optional<IconAtlasData> ParseIconAtlas(std::string_view json);
}
