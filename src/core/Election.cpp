#include "core/Election.h"
#include "b21ui/Abi.h"

#include <algorithm>
#include <cctype>

namespace b21ui::core {
    namespace {
        bool Valid(const Candidate& c) {
            return c.abiVersion >= 1 && c.rendezvousSize >= B21UI_RENDEZVOUS_MIN_SIZE;
        }
        bool NameLess(const std::string& a, const std::string& b) {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char l, char r) {
                return std::tolower(static_cast<unsigned char>(l)) < std::tolower(static_cast<unsigned char>(r));
            });
        }
    }

    std::optional<std::size_t> Elect(std::span<const Candidate> candidates) {
        std::optional<std::size_t> best;
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const auto& c = candidates[i];
            if (!Valid(c)) continue;
            if (!best) { best = i; continue; }
            const auto& b = candidates[*best];
            if (c.frameworkVersion > b.frameworkVersion ||
                (c.frameworkVersion == b.frameworkVersion && NameLess(c.moduleName, b.moduleName)))
                best = i;
        }
        return best;
    }
}
