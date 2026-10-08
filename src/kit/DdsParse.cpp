#include "kit/DdsParse.h"

#include <algorithm>
#include <cstring>

namespace b21ui::kit {
    namespace {
        std::uint32_t Read(std::span<const std::byte> b, std::size_t at) {
            std::uint32_t v{};
            std::memcpy(&v, b.data() + at, 4);
            return v;
        }
        constexpr std::uint32_t FourCC(char a, char b, char c, char d) {
            return static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8) |
                   (static_cast<std::uint32_t>(c) << 16) | (static_cast<std::uint32_t>(d) << 24);
        }
        // Bytes per 4x4 block for block-compressed formats; 0 for everything else.
        std::uint32_t BlockBytes(std::uint32_t format) {
            switch (format) {
            case 71: case 72: case 80: return 8;
            case 74: case 77: case 78: case 83: case 98: case 99: return 16;
            default: return 0;
            }
        }
        bool Uncompressed(std::uint32_t format) { return format == 28 || format == 29 || format == 87; }
    }

    std::optional<DdsImage> ParseDds(std::span<const std::byte> bytes) {
        if (bytes.size() < 128 || Read(bytes, 0) != 0x20534444 || Read(bytes, 4) != 124) return std::nullopt;
        DdsImage image;
        image.height = Read(bytes, 12);
        image.width = Read(bytes, 16);
        image.mipCount = std::max<std::uint32_t>(1, Read(bytes, 28));
        const auto pfFlags = Read(bytes, 80), fourcc = Read(bytes, 84), bits = Read(bytes, 88);
        std::size_t offset = 128;
        if ((pfFlags & 0x4) && fourcc == FourCC('D', 'X', '1', '0')) {
            if (bytes.size() < 148) return std::nullopt;
            image.dxgiFormat = Read(bytes, 128);
            offset = 148;
        } else if (pfFlags & 0x4) {
            switch (fourcc) {
            case FourCC('D', 'X', 'T', '1'): image.dxgiFormat = 71; break;
            case FourCC('D', 'X', 'T', '3'): image.dxgiFormat = 74; break;
            case FourCC('D', 'X', 'T', '5'): image.dxgiFormat = 77; break;
            case FourCC('A', 'T', 'I', '2'): case FourCC('B', 'C', '5', 'U'): image.dxgiFormat = 83; break;
            default: return std::nullopt;
            }
        } else if ((pfFlags & 0x40) && bits == 32) {
            image.dxgiFormat = Read(bytes, 92) == 0x000000FF ? 28u : 87u;
        } else {
            return std::nullopt;
        }
        const auto block = BlockBytes(image.dxgiFormat);
        if (!block && !Uncompressed(image.dxgiFormat)) return std::nullopt;
        if (!image.width || !image.height) return std::nullopt;
        std::uint32_t w = image.width, h = image.height;
        for (std::uint32_t level = 0; level < image.mipCount; ++level) {
            DdsSubresource mip{offset, 0, 0, w, h};
            if (block) {
                mip.rowPitch = std::max(1u, (w + 3) / 4) * block;
                mip.slicePitch = mip.rowPitch * std::max(1u, (h + 3) / 4);
            } else {
                mip.rowPitch = w * 4;
                mip.slicePitch = mip.rowPitch * h;
            }
            offset += mip.slicePitch;
            if (offset > bytes.size()) return std::nullopt;
            image.mips.push_back(mip);
            w = std::max(1u, w / 2);
            h = std::max(1u, h / 2);
        }
        return image;
    }
}
