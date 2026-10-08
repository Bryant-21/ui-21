#pragma once
#include "b21ui/Texture.h"

#include <imgui.h>

#include <memory>
#include <string_view>

namespace b21ui {
    struct Palette4 {
        ImU32 shadow{}, dim{}, mid{}, bright{};
    };

    class IconAtlas {
    public:
        IconAtlas();
        ~IconAtlas();
        bool Load(ID3D11Device* device, const std::filesystem::path& jsonFile);
        void Release();
        [[nodiscard]] bool Has(std::string_view symbol) const;
        // Draws one marker icon centered at `center`. Tintable icons are drawn as four band masks
        // tinted with `palette`; others use their full-color cell. Optional offset black shadow.
        bool Draw(ImDrawList* list, std::string_view symbol, ImVec2 center, float size, const Palette4& palette,
                  float alpha = 1.0F, bool shadow = true) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
