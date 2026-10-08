#include "b21ui/IconAtlas.h"
#include "kit/IconAtlasData.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace b21ui {
    struct IconAtlas::Impl {
        kit::IconAtlasData data;
        Texture texture;
    };

    IconAtlas::IconAtlas() : impl_(std::make_unique<Impl>()) {}
    IconAtlas::~IconAtlas() { Release(); }

    bool IconAtlas::Load(ID3D11Device* device, const std::filesystem::path& jsonFile) {
        Release();
        std::ifstream in(jsonFile);
        if (!in) return false;
        std::stringstream text;
        text << in.rdbuf();
        auto data = kit::ParseIconAtlas(text.str());
        if (!data) return false;
        auto texture = LoadDds(device, jsonFile.parent_path() / data->texture);
        if (!texture) return false;
        impl_->data = std::move(*data);
        impl_->texture = *texture;
        return true;
    }

    void IconAtlas::Release() {
        impl_->texture.Release();
        impl_->data = {};
    }

    bool IconAtlas::Has(std::string_view symbol) const {
        return impl_->texture && impl_->data.icons.contains(std::string(symbol));
    }

    bool IconAtlas::Draw(ImDrawList* list, std::string_view symbol, ImVec2 center, float size, const Palette4& palette,
                         float alpha, bool shadow) const {
        const auto found = impl_->data.icons.find(std::string(symbol));
        if (!impl_->texture || found == impl_->data.icons.end()) return false;
        const auto& icon = found->second;
        const auto id = impl_->texture.Id();
        const ImVec2 half{size * 0.5F, size * 0.5F};
        const ImVec2 min{center.x - half.x, center.y - half.y}, max{center.x + half.x, center.y + half.y};
        const auto withAlpha = [alpha](ImU32 c) {
            const auto a = static_cast<ImU32>(((c >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
            return (c & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
        };
        const auto quad = [&](const kit::IconRect& r, ImVec2 offset, ImU32 color) {
            list->AddImage(id, {min.x + offset.x, min.y + offset.y}, {max.x + offset.x, max.y + offset.y},
                           {r.u0, r.v0}, {r.u1, r.v1}, withAlpha(color));
        };
        const float shift = std::max(1.0F, size * 0.04F);
        if (shadow) quad(icon.shape, {shift, shift}, IM_COL32(0, 0, 0, 204));
        if (!icon.tintable) {
            quad(icon.shape, {0, 0}, IM_COL32_WHITE);
            return true;
        }
        const ImU32 tones[4]{palette.shadow, palette.dim, palette.mid, palette.bright};
        for (int i = 0; i < 4; ++i) quad(icon.bands[i], {0, 0}, tones[i]);
        return true;
    }
}
