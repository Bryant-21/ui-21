#pragma once
#include "b21ui/Abi.h"
#include "b21ui/Keybindings.h"
#include <mutex>
#include <nlohmann/json.hpp>

namespace b21ui::keys {
    enum class Collision { None, Possible, Overlap };
    struct Conflict { std::size_t first{}, second{}; Collision kind{}; };
    struct Source { std::string id, label, status; std::size_t count{}; };
    struct Snapshot {
        std::vector<Binding> bindings;
        std::vector<Conflict> conflicts;
        std::vector<Source> sources;
        std::vector<std::string> undisclosed;
    };
    Collision Compare(const Binding& first, const Binding& second);
    std::vector<Conflict> Conflicts(const std::vector<Binding>& bindings);
    bool InContext(const Binding& binding, std::string_view context);
    void DescribeGameActivation(Binding& binding, std::string_view context);
    nlohmann::json Encode(const std::vector<Binding>& bindings);
    std::vector<Binding> Decode(const nlohmann::json& json, std::string_view source);

    class Providers {
    public:
        bool Add(const B21UI_KeybindingProvider& provider);
        Snapshot Read() const;
    private:
        struct Provider { std::string id, label; void* user{}; const char* (*snapshot)(void*){}; };
        mutable std::mutex mutex_;
        std::vector<Provider> providers_;
    };
}
