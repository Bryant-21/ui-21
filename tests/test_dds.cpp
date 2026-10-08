#include <doctest/doctest.h>
#include "kit/DdsParse.h"

#include <cstring>

using b21ui::kit::ParseDds;

namespace {
    void Put(std::vector<std::byte>& b, std::size_t at, std::uint32_t v) { std::memcpy(b.data() + at, &v, 4); }

    std::vector<std::byte> Header(std::uint32_t w, std::uint32_t h, std::uint32_t mips, std::uint32_t fourcc,
                                  std::uint32_t dxgi, std::size_t payload) {
        const std::size_t head = dxgi ? 148 : 128;
        std::vector<std::byte> b(head + payload);
        Put(b, 0, 0x20534444);
        Put(b, 4, 124);
        Put(b, 8, 0x1007 | 0x20000);
        Put(b, 12, h);
        Put(b, 16, w);
        Put(b, 28, mips);
        Put(b, 76, 32);
        Put(b, 80, 0x4);
        Put(b, 84, dxgi ? 0x30315844 : fourcc);
        if (dxgi) {
            Put(b, 128, dxgi);
            Put(b, 132, 3);
            Put(b, 140, 1);
        }
        return b;
    }
}

TEST_CASE("BC7 with a DX10 header and a full mip chain") {
    const auto bytes = Header(8, 8, 4, 0, 98, 112);
    const auto dds = ParseDds(bytes);
    REQUIRE(dds);
    CHECK(dds->width == 8u);
    CHECK(dds->dxgiFormat == 98u);
    REQUIRE(dds->mips.size() == 4);
    CHECK(dds->mips[0].offset == 148u);
    CHECK(dds->mips[0].rowPitch == 32u);
    CHECK(dds->mips[0].slicePitch == 64u);
    CHECK(dds->mips[1].offset == 212u);
    CHECK(dds->mips[3].offset == 244u);
    CHECK(dds->mips[3].width == 1u);
}

TEST_CASE("legacy DXT1 maps to BC1") {
    const auto dds = ParseDds(Header(4, 4, 1, 0x31545844, 0, 8));
    REQUIRE(dds);
    CHECK(dds->dxgiFormat == 71u);
    CHECK(dds->mips[0].offset == 128u);
    CHECK(dds->mips[0].slicePitch == 8u);
}

TEST_CASE("truncated payloads and bad magic are rejected") {
    CHECK_FALSE(ParseDds(Header(8, 8, 4, 0, 98, 100)));
    auto bad = Header(4, 4, 1, 0x31545844, 0, 8);
    bad[0] = std::byte{0};
    CHECK_FALSE(ParseDds(bad));
    CHECK_FALSE(ParseDds(std::vector<std::byte>(10)));
}

TEST_CASE("unsupported formats are rejected") {
    CHECK_FALSE(ParseDds(Header(4, 4, 1, 0, 2, 256)));
}

TEST_CASE("zero mip count means one level") {
    const auto dds = ParseDds(Header(4, 4, 0, 0x35545844, 0, 16));
    REQUIRE(dds);
    CHECK(dds->dxgiFormat == 77u);
    CHECK(dds->mips.size() == 1);
}
