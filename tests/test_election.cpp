#include <doctest/doctest.h>
#include "b21ui/Abi.h"
#include "core/Election.h"

#include <vector>

using b21ui::core::Candidate;
using b21ui::core::Elect;

namespace {
    Candidate Make(std::uint32_t version, std::string name) {
        return {version, B21UI_ABI_VERSION, B21UI_RENDEZVOUS_MIN_SIZE, std::move(name)};
    }
}

TEST_CASE("no candidates elects nobody") {
    CHECK_FALSE(Elect({}).has_value());
}

TEST_CASE("highest framework version wins") {
    const std::vector<Candidate> c{Make(1, "B21_A.dll"), Make(3, "B21_B.dll"), Make(2, "B21_C.dll")};
    CHECK(Elect(c) == 1u);
}

TEST_CASE("version tie goes to the case-insensitively smallest module name") {
    const std::vector<Candidate> c{Make(2, "b21_zeta.dll"), Make(2, "B21_Alpha.dll"), Make(2, "B21_beta.dll")};
    CHECK(Elect(c) == 1u);
}

TEST_CASE("invalid candidates are skipped") {
    auto noAbi = Make(9, "B21_NoAbi.dll");
    noAbi.abiVersion = 0;
    auto tiny = Make(9, "B21_Tiny.dll");
    tiny.rendezvousSize = 4;
    const std::vector<Candidate> c{noAbi, tiny, Make(1, "B21_Ok.dll")};
    CHECK(Elect(c) == 2u);
}

TEST_CASE("all invalid elects nobody") {
    auto bad = Make(1, "B21_Bad.dll");
    bad.abiVersion = 0;
    CHECK_FALSE(Elect(std::vector<Candidate>{bad}).has_value());
}
