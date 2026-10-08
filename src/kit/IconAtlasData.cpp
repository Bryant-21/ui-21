#include "kit/IconAtlasData.h"

#include <nlohmann/json.hpp>

namespace b21ui::kit {
    namespace {
        bool Rect(const nlohmann::json& j, IconRect& out) {
            if (!j.is_array() || j.size() != 4) return false;
            for (const auto& v : j) if (!v.is_number()) return false;
            out = {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()};
            return true;
        }
    }

    std::optional<IconAtlasData> ParseIconAtlas(std::string_view json) {
        const auto root = nlohmann::json::parse(json, nullptr, false);
        if (!root.is_object() || root.value("version", 0) != 1) return std::nullopt;
        IconAtlasData data;
        data.texture = root.value("texture", "");
        data.width = root.value("width", 0);
        data.height = root.value("height", 0);
        if (data.texture.empty() || data.width <= 0 || data.height <= 0) return std::nullopt;
        const auto icons = root.find("icons");
        if (icons == root.end() || !icons->is_object()) return std::nullopt;
        for (const auto& [name, value] : icons->items()) {
            IconEntry entry;
            entry.tintable = value.value("tintable", false);
            if (!Rect(value.value("shape", nlohmann::json()), entry.shape)) return std::nullopt;
            const auto bands = value.value("bands", nlohmann::json::array());
            if (entry.tintable) {
                if (bands.size() != 4) return std::nullopt;
                for (std::size_t i = 0; i < 4; ++i)
                    if (!Rect(bands[i], entry.bands[i])) return std::nullopt;
            }
            data.icons.emplace(name, entry);
        }
        return data;
    }
}
