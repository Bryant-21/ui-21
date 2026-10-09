#pragma once
#include "b21ui/Abi.h"

#include <cstdint>
#include <deque>
#include <string>

namespace b21ui::core {
    class SettingsRegistry {
    public:
        void Add(B21UI_ClientId id, const B21UI_ClientDesc& desc);
        bool Contains(B21UI_ClientId id) const;
        B21UI_ClientId Resolve(B21UI_ClientId id = 0) const;
        void Select(B21UI_ClientId id);
        const char* Category(B21UI_ClientId id) const;
        std::uint32_t Panels(B21UI_SettingsPanel* panels, std::uint32_t capacity) const;

    private:
        struct Panel {
            B21UI_ClientId id;
            std::string label;
            std::string icon;
            std::string category;
        };
        std::deque<Panel> panels_;
        B21UI_ClientId selected_{};
    };
}
