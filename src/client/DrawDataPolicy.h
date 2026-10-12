#pragma once
#include <imgui.h>

#include <algorithm>

namespace b21ui::client {
    // ImGui_ImplDX11_RenderDrawData with no command lists and no pending texture requests draws nothing,
    // yet still maps its buffers and saves, sets and restores the pipeline state. Command lists are counted
    // rather than vertices so callback-only lists still reach the backend; the texture test is the backend's
    // own, so atlas creation, updates and destruction are never held back.
    [[nodiscard]] inline bool NeedsRenderDrawData(const ImDrawData* drawData) {
        if (!drawData) return false;
        if (drawData->CmdListsCount > 0) return true;
        return drawData->Textures && std::any_of(drawData->Textures->begin(), drawData->Textures->end(),
            [](const ImTextureData* texture) { return texture->Status != ImTextureStatus_OK; });
    }
}
