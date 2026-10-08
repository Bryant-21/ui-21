#pragma once
#include <imgui.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace b21ui {
    struct Texture {
        ID3D11ShaderResourceView* srv{};
        int width{};
        int height{};
        [[nodiscard]] ImTextureID Id() const { return reinterpret_cast<ImTextureID>(srv); }
        [[nodiscard]] explicit operator bool() const { return srv != nullptr; }
        void Release();
    };

    std::optional<Texture> LoadDds(ID3D11Device* device, const std::filesystem::path& file);
    std::optional<Texture> LoadDds(ID3D11Device* device, std::span<const std::byte> bytes);
    std::optional<Texture> LoadImageFile(ID3D11Device* device, const std::filesystem::path& file);   // PNG/JPG/BMP
    std::optional<Texture> CreateRgbaTexture(ID3D11Device* device, int width, int height, const std::uint8_t* rgba);
}
