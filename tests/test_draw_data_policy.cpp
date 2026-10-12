#include <doctest/doctest.h>
#include "client/DrawDataPolicy.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>

#include <initializer_list>

using b21ui::client::NeedsRenderDrawData;

namespace {
    using Microsoft::WRL::ComPtr;

    void MarkCalled(const ImDrawList*, const ImDrawCmd* command) { *static_cast<bool*>(command->UserCallbackData) = true; }

    // Shuts the backend and context down even when a REQUIRE aborts the test part-way.
    struct BackendGuard {
        ImGuiContext* previous = ImGui::GetCurrentContext();
        ImGuiContext* imgui = ImGui::CreateContext();
        BackendGuard() { ImGui::SetCurrentContext(imgui); }
        ~BackendGuard() {
            ImGui::SetCurrentContext(imgui);
            if (ImGui::GetIO().BackendRendererUserData)
                ImGui_ImplDX11_Shutdown();
            ImGui::DestroyContext(imgui);
            ImGui::SetCurrentContext(previous);
        }
    };
}

TEST_CASE("draw data policy: no draw data or an empty frame skips the render") {
    CHECK_FALSE(NeedsRenderDrawData(nullptr));
    ImDrawData empty;
    CHECK_FALSE(NeedsRenderDrawData(&empty));
    ImVector<ImTextureData*> none;
    empty.Textures = &none;
    CHECK_FALSE(NeedsRenderDrawData(&empty));
}

TEST_CASE("draw data policy: any command list renders, even one with no vertices") {
    ImDrawData callbackOnly;
    callbackOnly.CmdListsCount = 1;
    CHECK(callbackOnly.TotalVtxCount == 0);
    CHECK(NeedsRenderDrawData(&callbackOnly));
}

TEST_CASE("draw data policy: a texture request reaches the backend with nothing to draw") {
    ImTextureData atlas, icons;
    ImVector<ImTextureData*> textures;
    textures.push_back(&atlas);
    textures.push_back(&icons);
    ImDrawData data;
    data.Textures = &textures;
    atlas.Status = icons.Status = ImTextureStatus_OK;
    CHECK_FALSE(NeedsRenderDrawData(&data));
    for (const auto status : {ImTextureStatus_WantCreate, ImTextureStatus_WantUpdates, ImTextureStatus_WantDestroy,
             ImTextureStatus_Destroyed}) {
        CAPTURE(static_cast<int>(status));
        icons.Status = status;
        CHECK(NeedsRenderDrawData(&data));
    }
    icons.Status = ImTextureStatus_OK;
    CHECK_FALSE(NeedsRenderDrawData(&data));
}

TEST_CASE("draw data policy: the DX11 backend skips only idle frames") {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &device, nullptr, &context)));
    BackendGuard guard;
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().DisplaySize = {64, 64};
    REQUIRE(ImGui_ImplDX11_Init(device.Get(), context.Get()));
    const auto frame = [](auto&& draw) {
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        draw();
        ImGui::Render();
        return ImGui::GetDrawData();
    };

    // The first frame creates the font atlas even though nothing is drawn.
    auto* first = frame([] {});
    REQUIRE(first);
    CHECK(first->CmdListsCount == 0);
    CHECK(NeedsRenderDrawData(first));
    for (int i = 0; i < 4 && NeedsRenderDrawData(ImGui::GetDrawData()); ++i) {
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        frame([] {});
    }
    auto* idle = ImGui::GetDrawData();
    REQUIRE(idle);
    CHECK(idle->CmdListsCount == 0);
    CHECK_FALSE(NeedsRenderDrawData(idle));

    bool called = false;
    auto* callbackOnly = frame([&] { ImGui::GetBackgroundDrawList()->AddCallback(MarkCalled, &called); });
    REQUIRE(callbackOnly);
    CHECK(callbackOnly->TotalVtxCount == 0);
    REQUIRE(NeedsRenderDrawData(callbackOnly));
    ImGui_ImplDX11_RenderDrawData(callbackOnly);
    CHECK(called);

    auto* drawn = frame([] { ImGui::GetBackgroundDrawList()->AddRectFilled({0, 0}, {32, 32}, IM_COL32_WHITE); });
    REQUIRE(drawn);
    CHECK(drawn->TotalVtxCount > 0);
    CHECK(NeedsRenderDrawData(drawn));
    context->ClearState();
}
