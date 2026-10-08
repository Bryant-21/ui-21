#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace b21ui::core {
    struct Candidate {
        std::uint32_t frameworkVersion{};
        std::uint32_t abiVersion{};
        std::uint32_t rendezvousSize{};
        std::string moduleName;
    };

    // Every module runs this over the same candidate set, so all of them agree on the host
    // without any messaging or load-order dependency.
    std::optional<std::size_t> Elect(std::span<const Candidate> candidates);
}
