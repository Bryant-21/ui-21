#pragma once
#include <functional>
#include <string>
#include <vector>

namespace b21ui::keys {
    struct Binding {
        std::string id, label, source;
        int key{}, modifiers{};
        bool exactModifiers{};
        std::vector<std::string> contexts;
        std::string trigger = "unknown", conditions;
    };

    // Macro codes: keyboard scan codes, mouse 256..265, Xbox 266..281. Zero is unbound.
    // Empty contexts mean unknown scope; "*" means all states. Callbacks run on the game thread.
    bool Register(const char* provider, const char* label, std::function<std::vector<Binding>()> snapshot);
    int FromVirtualKey(unsigned int key);
    int FromGamepadMask(unsigned int mask);
    std::string KeyName(int key);
    std::string ChordName(const Binding& binding);
}
