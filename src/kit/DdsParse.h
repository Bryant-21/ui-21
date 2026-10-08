#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace b21ui::kit {
    struct DdsSubresource {
        std::size_t offset{};
        std::uint32_t rowPitch{};
        std::uint32_t slicePitch{};
        std::uint32_t width{};
        std::uint32_t height{};
    };

    struct DdsImage {
        std::uint32_t width{};
        std::uint32_t height{};
        std::uint32_t mipCount{};
        std::uint32_t dxgiFormat{};
        std::vector<DdsSubresource> mips;
    };

    std::optional<DdsImage> ParseDds(std::span<const std::byte> bytes);
}
