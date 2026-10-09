#include <doctest/doctest.h>

#include "b21ui/modern/Layout.h"

using b21ui::modern::layout::SliderLimitsFit;

TEST_CASE("slider limits: both ends sit inside the frame when they clear the centred value") {
    // 300 wide, value 40 wide: 130 each side, ends of 20 and 30 plus 6 padding either side fit.
    CHECK(SliderLimitsFit(300.0F, 20.0F, 30.0F, 40.0F, 6.0F));
}

TEST_CASE("slider limits: a frame too narrow for them moves them out") {
    // 100 wide, value 40: 30 each side, a 30-wide end needs 42.
    CHECK_FALSE(SliderLimitsFit(100.0F, 30.0F, 30.0F, 40.0F, 6.0F));
}

TEST_CASE("slider limits: the wider end decides, so both stay or both go") {
    // 200 wide, value 40: 80 each side; the 10-wide end fits, the 70-wide one needs 82.
    CHECK_FALSE(SliderLimitsFit(200.0F, 10.0F, 70.0F, 40.0F, 6.0F));
    CHECK(SliderLimitsFit(204.0F, 10.0F, 70.0F, 40.0F, 6.0F));
}
