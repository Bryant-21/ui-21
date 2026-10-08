#include <doctest/doctest.h>
#include "kit/IconAtlasData.h"

using b21ui::kit::ParseIconAtlas;

TEST_CASE("atlas JSON parses tintable and plain icons") {
    const auto atlas = ParseIconAtlas(R"({"version":1,"texture":"markers.dds","width":256,"height":128,"cell":128,
        "icons":{"FarmMarker":{"tintable":true,"bands":[[0,0,0.25,0.5],[0.25,0,0.5,0.5],[0.5,0,0.75,0.5],[0.75,0,1,0.5]],"shape":[0,0.5,0.25,1]},
                 "custom_skull":{"tintable":false,"bands":[],"shape":[0.25,0.5,0.5,1]}}})");
    REQUIRE(atlas);
    CHECK(atlas->texture == "markers.dds");
    CHECK(atlas->icons.size() == 2);
    const auto& farm = atlas->icons.at("FarmMarker");
    CHECK(farm.tintable);
    CHECK(farm.bands[3].u0 == doctest::Approx(0.75F));
    CHECK_FALSE(atlas->icons.at("custom_skull").tintable);
}

TEST_CASE("wrong version, malformed JSON and bad rects are rejected") {
    CHECK_FALSE(ParseIconAtlas(R"({"version":2,"texture":"x.dds","width":1,"height":1,"icons":{}})"));
    CHECK_FALSE(ParseIconAtlas("{not json"));
    CHECK_FALSE(ParseIconAtlas(R"({"version":1,"texture":"x.dds","width":1,"height":1,
        "icons":{"A":{"tintable":true,"bands":[[0,0,1]],"shape":[0,0,1,1]}}})"));
}
