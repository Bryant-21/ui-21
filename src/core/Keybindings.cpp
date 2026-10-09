#include "core/Keybindings.h"
#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>

namespace b21ui::keys {
    bool InContext(const Binding& binding, std::string_view context) {
        return context.empty() || binding.contexts.empty() ||
            std::ranges::find(binding.contexts, "*") != binding.contexts.end() ||
            std::ranges::find(binding.contexts, context) != binding.contexts.end();
    }
    Collision Compare(const Binding& a, const Binding& b) {
        if (!a.key || a.key != b.key || (a.source == b.source && a.id == b.id)) return Collision::None;
        if (a.exactModifiers && b.exactModifiers && a.modifiers != b.modifiers) return Collision::None;
        if (a.exactModifiers && (a.modifiers & b.modifiers) != b.modifiers) return Collision::None;
        if (b.exactModifiers && (b.modifiers & a.modifiers) != a.modifiers) return Collision::None;
        bool known = !a.contexts.empty() && !b.contexts.empty();
        if (known && !std::ranges::any_of(a.contexts, [&](const auto& context) { return InContext(b, context) || context == "*"; }))
            return Collision::None;
        return !known || a.trigger == "unknown" || b.trigger == "unknown" || a.trigger != b.trigger || !a.conditions.empty() || !b.conditions.empty() ?
            Collision::Possible : Collision::Overlap;
    }
    void DescribeGameActivation(Binding& binding, std::string_view context) {
        binding.trigger = "unknown";
        binding.conditions = "The control table declares the assignment; this game/menu handler's activation is not cataloged.";
        if (context != "Gameplay") return;
        struct Rule { const char* event; const char* trigger; const char* details; };
        static constexpr Rule rules[]{
            {"ReadyWeapon", "tap / hold", "Tap/release: ready or reload. Hold: ready/holster. Weapon and target state affect the action."},
            {"Melee", "tap / hold + release", "Tap/release: melee. Hold: arm a grenade; release: throw. Equipped weapon and perks affect the action."},
            {"Pipboy", "tap / hold", "Tap/release: open Pip-Boy. Hold: toggle its light. Power armor and command mode can alter the action."},
            {"TogglePOV", "tap / hold", "Tap: switch view. Hold: free look or enter a nearby workshop. Player movement and workshop availability affect the action."},
            {"Activate", "tap / hold", "Tap/release: activate. Hold: grab an object or exit power armor. Target and player state affect the action."},
            {"Jump", "press / hold", "Press: jump. Hold: alternate furniture exit when available."},
            {"Forward", "hold", "Movement continues while held."},
            {"Back", "hold", "Movement continues while held."},
            {"StrafeLeft", "hold", "Movement continues while held."},
            {"StrafeRight", "hold", "Movement continues while held."},
            {"PrimaryAttack", "press / hold / release", "Weapon-specific attack handling; automatic fire and charged attacks can use holds and release."},
            {"SecondaryAttack", "press / hold / release", "Weapon-specific aim/block handling; behavior can depend on holds and release."}
        };
        for (const auto& rule : rules) if (binding.id == rule.event) {
            binding.trigger = rule.trigger; binding.conditions = rule.details; return;
        }
    }
    std::vector<Conflict> Conflicts(const std::vector<Binding>& bindings) {
        std::vector<Conflict> result;
        for (std::size_t i = 0; i < bindings.size(); ++i) for (std::size_t j = i + 1; j < bindings.size(); ++j)
            if (const auto kind = Compare(bindings[i], bindings[j]); kind != Collision::None) result.push_back({i, j, kind});
        return result;
    }
    nlohmann::json Encode(const std::vector<Binding>& bindings) {
        auto out = nlohmann::json::array();
        for (const auto& b : bindings) out.push_back({{"id", b.id}, {"label", b.label}, {"source", b.source},
            {"key", b.key}, {"modifiers", b.modifiers}, {"exactModifiers", b.exactModifiers},
            {"contexts", b.contexts}, {"trigger", b.trigger}, {"conditions", b.conditions}});
        return out;
    }
    std::vector<Binding> Decode(const nlohmann::json& json, std::string_view source) {
        if (!json.is_array()) throw std::runtime_error("Expected a binding array");
        std::vector<Binding> result;
        for (const auto& row : json) {
            Binding b;
            b.id = row.at("id").get<std::string>(); b.label = row.at("label").get<std::string>();
            b.source = row.value("source", std::string(source));
            if (b.source.empty()) b.source = source;
            b.key = row.at("key").get<int>(); b.modifiers = row.value("modifiers", 0);
            b.exactModifiers = row.value("exactModifiers", false);
            b.contexts = row.value("contexts", std::vector<std::string>{});
            b.trigger = row.value("trigger", "unknown"); b.conditions = row.value("conditions", "");
            if (b.trigger.empty()) b.trigger = "unknown";
            if (b.id.empty() || b.label.empty() || b.key < 0 || b.key > 66559 || (b.key > 283 && b.key < 1024) || b.modifiers < 0 || b.modifiers > 7)
                throw std::runtime_error("Invalid binding descriptor");
            if (b.key) result.push_back(std::move(b));
        }
        return result;
    }
    bool Providers::Add(const B21UI_KeybindingProvider& p) {
        if (p.size < sizeof(p) || !p.id || !*p.id || !p.snapshot) return false;
        std::scoped_lock lock(mutex_);
        if (std::ranges::any_of(providers_, [&](const auto& item) { return item.id == p.id; })) return false;
        providers_.push_back({p.id, p.label ? p.label : p.id, p.user, p.snapshot}); return true;
    }
    Snapshot Providers::Read() const {
        std::vector<Provider> providers;
        { std::scoped_lock lock(mutex_); providers = providers_; }
        Snapshot result;
        for (const auto& p : providers) {
            Source source{p.id, p.label, "Published", 0};
            try {
                const auto* text = p.snapshot(p.user);
                auto bindings = Decode(nlohmann::json::parse(text ? text : "null"), p.label);
                source.count = bindings.size();
                result.bindings.insert(result.bindings.end(), bindings.begin(), bindings.end());
            } catch (const std::exception& e) { source.status = std::string("Unavailable: ") + e.what(); }
            catch (...) { source.status = "Unavailable: provider failed"; }
            result.sources.push_back(std::move(source));
        }
        return result;
    }
    std::string KeyName(int key) {
        static const std::array<const char*, 28> extended{"Home", "Up", "PgUp", "", "Left", "", "Right", "", "End", "Down", "PgDn", "Ins", "Del",
            "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
        static const char* basic[] = {"Unbound", "Esc", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Backspace", "Tab",
            "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Ctrl", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`", "Shift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "RShift", "Num *", "Alt", "Space", "Caps", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "NumLock", "Scroll", "Num7", "Num8", "Num9", "Num -", "Num4", "Num5", "Num6", "Num +", "Num1", "Num2", "Num3", "Num0", "Num ."};
        static const char* mouse[]{"Mouse 1", "Mouse 2", "Mouse 3", "Mouse 4", "Mouse 5", "Mouse 6", "Mouse 7", "Mouse 8", "Wheel up", "Wheel down"};
        static const char* pad[]{"D-pad up", "D-pad down", "D-pad left", "D-pad right", "Menu", "View", "L3", "R3", "LB", "RB", "A", "B", "X", "Y", "LT", "RT", "Left stick", "Right stick"};
        if (key >= 0 && key < static_cast<int>(std::size(basic))) return basic[key];
        if (key >= 256 && key <= 265) return mouse[key - 256];
        if (key >= 266 && key <= 283) return pad[key - 266];
        if (key >= 199 && key <= 211 && *extended[key - 199]) return extended[key - 199];
        if (key >= 1024) return std::format("Xbox input {:X}",key-1024);
        switch (key) {
        case 87: return "F11"; case 88: return "F12"; case 156: return "Num Enter"; case 157: return "RCtrl";
        case 181: return "Num /"; case 183: return "PrtSc"; case 184: return "RAlt"; case 197: return "Pause";
        case 219: return "Win"; case 220: return "RWin"; case 221: return "Menu";
        default: return std::format("Scan {:02X}", key);
        }
    }
    std::string ChordName(const Binding& b) {
        std::string text;
        if (b.modifiers & 1) text += "Shift + ";
        if (b.modifiers & 2) text += "Ctrl + ";
        if (b.modifiers & 4) text += "Alt + ";
        return text + KeyName(b.key);
    }
    int FromGamepadMask(unsigned int mask) {
        mask &= 0xFFFF;
        if (!mask || mask==255) return 0;
        static const unsigned int buttons[]{1,2,4,8,16,32,64,128,256,512,4096,8192,16384,32768,9,10};
        const auto found=std::ranges::find(buttons,mask);
        return found==std::end(buttons) ? static_cast<int>(1024+mask) : 266+static_cast<int>(found-std::begin(buttons));
    }
}
