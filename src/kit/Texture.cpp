#include "b21ui/Texture.h"
#include "kit/DdsParse.h"

#include <d3d11.h>
#include <stb_image.h>

#include <fstream>
#include <vector>

namespace b21ui {
    void Texture::Release() {
        if (srv) srv->Release();
        srv = nullptr;
    }

    namespace {
        std::vector<std::byte> ReadAll(const std::filesystem::path& file) {
            std::ifstream in(file, std::ios::binary | std::ios::ate);
            if (!in) return {};
            std::vector<std::byte> bytes(static_cast<std::size_t>(in.tellg()));
            in.seekg(0);
            in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            return in ? bytes : std::vector<std::byte>{};
        }

        std::optional<Texture> Create(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& desc,
                                      const std::vector<D3D11_SUBRESOURCE_DATA>& data) {
            ID3D11Texture2D* texture{};
            if (FAILED(device->CreateTexture2D(&desc, data.data(), &texture)) || !texture) return std::nullopt;
            ID3D11ShaderResourceView* view{};
            const auto hr = device->CreateShaderResourceView(texture, nullptr, &view);
            texture->Release();
            if (FAILED(hr) || !view) return std::nullopt;
            return Texture{view, static_cast<int>(desc.Width), static_cast<int>(desc.Height)};
        }
    }

    std::optional<Texture> LoadDds(ID3D11Device* device, const std::filesystem::path& file) {
        return LoadDds(device, std::span<const std::byte>(ReadAll(file)));
    }

    std::optional<Texture> LoadDds(ID3D11Device* device, std::span<const std::byte> bytes) {
        if (!device) return std::nullopt;
        const auto dds = kit::ParseDds(bytes);
        if (!dds) return std::nullopt;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = dds->width;
        desc.Height = dds->height;
        desc.MipLevels = static_cast<UINT>(dds->mips.size());
        desc.ArraySize = 1;
        desc.Format = static_cast<DXGI_FORMAT>(dds->dxgiFormat);
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        std::vector<D3D11_SUBRESOURCE_DATA> data;
        for (const auto& mip : dds->mips)
            data.push_back({bytes.data() + mip.offset, mip.rowPitch, mip.slicePitch});
        return Create(device, desc, data);
    }

    std::optional<Texture> CreateRgbaTexture(ID3D11Device* device, int width, int height, const std::uint8_t* rgba) {
        if (!device || !rgba || width <= 0 || height <= 0) return std::nullopt;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        return Create(device, desc, {{rgba, static_cast<UINT>(width * 4), 0}});
    }

    std::optional<Texture> LoadImageFile(ID3D11Device* device, const std::filesystem::path& file) {
        const auto bytes = ReadAll(file);
        if (bytes.empty()) return std::nullopt;
        int w{}, h{}, channels{};
        auto* pixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()),
                                             static_cast<int>(bytes.size()), &w, &h, &channels, 4);
        if (!pixels) return std::nullopt;
        auto texture = CreateRgbaTexture(device, w, h, pixels);
        stbi_image_free(pixels);
        return texture;
    }
}
