#pragma once
#include "core/Keybindings.h"
#include <array>
#include <imgui.h>

namespace b21ui::keys {
    class View {
    public:
        void Navigation();
        bool Draw(const Snapshot& snapshot);
    private:
        bool coverage_{}, gamepad_{}, conflictsOnly_{}, showMap_{};
        std::string context_, source_;
        int selectedKey_ = -1;
        std::array<char, 128> search_{};
        std::vector<Collision> levels_;
        bool diagramClicked_{};
        void Map(const Snapshot& snapshot, const std::vector<std::size_t>& visible);
        void Key(const Snapshot& snapshot, const std::vector<std::size_t>& visible, int code, float x, float y, float width, float height);
        void Coverage(const Snapshot& snapshot);
        void Controller(const Snapshot& snapshot, const std::vector<std::size_t>& visible, float width, float height);
        void Mouse(const Snapshot& snapshot, const std::vector<std::size_t>& visible, ImVec2 origin, float scale);
        void DiagramButton(const Snapshot& snapshot, const std::vector<std::size_t>& visible, int code,
            const std::vector<ImVec2>& shape, const char* label, ImU32 ink = 0);
    };
}
