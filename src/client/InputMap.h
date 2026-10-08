#pragma once
#include "b21ui/Abi.h"

#include <cstdint>
#include <imgui.h>

namespace b21ui::client {
    struct StickState {
        float lx{};
        float ly{};
        float rx{};
        float ry{};
    };

    ImGuiKey KeyFromVk(std::uint32_t vk);
    ImGuiKey KeyFromPad(std::uint32_t padButton);
    void ApplyEvents(ImGuiIO& io, const B21UI_Frame& frame, StickState& sticks, float deadzone = 0.2F);
}
