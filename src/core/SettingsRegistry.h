#pragma once
#include "b21ui/Abi.h"

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>

namespace b21ui::core {
    class SettingsRegistry {
    public:
        // MOD SETTINGS replaces the pause menu, so a settings panel pauses whatever its flag says.
        static bool PausesGame(const B21UI_ClientDesc& desc) {
            return (desc.flags & B21UI_FLAG_PAUSES_GAME) != 0 ||
                   ((desc.flags & B21UI_FLAG_SETTINGS) != 0 && desc.kind == B21UI_KIND_MODAL);
        }
        void Add(B21UI_ClientId id, const B21UI_ClientDesc& desc);
        bool Contains(B21UI_ClientId id) const;
        B21UI_ClientId Resolve(B21UI_ClientId id = 0) const;
        void Select(B21UI_ClientId id);
        // Stands in for the selection until one is made, so a panel from an earlier session reopens.
        void Prefer(std::string_view label) { preferred_ = label; }
        const char* Label(B21UI_ClientId id) const;
        const char* Category(B21UI_ClientId id) const;
        std::uint32_t Panels(B21UI_SettingsPanel* panels, std::uint32_t capacity) const;

    private:
        B21UI_ClientId First() const;
        struct Panel {
            B21UI_ClientId id;
            std::string label;
            std::string icon;
            std::string category;
        };
        std::deque<Panel> panels_;
        B21UI_ClientId selected_{};
        std::string preferred_;
    };
}
