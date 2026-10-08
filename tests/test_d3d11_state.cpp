#include <doctest/doctest.h>
#include "client/D3D11State.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>

#include <array>
#include <cstring>

namespace {
    using Microsoft::WRL::ComPtr;

    struct Graphics {
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        Graphics() {
            const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
            REQUIRE(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                levels, 2, D3D11_SDK_VERSION, &device, nullptr, &context)));
        }
        ComPtr<ID3D11Texture2D> Texture(UINT bindings) {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = desc.Height = 64;
            desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.BindFlags = bindings;
            ComPtr<ID3D11Texture2D> texture;
            REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &texture)));
            return texture;
        }
        ComPtr<ID3D11RenderTargetView> Target() {
            auto texture = Texture(D3D11_BIND_RENDER_TARGET);
            ComPtr<ID3D11RenderTargetView> target;
            REQUIRE(SUCCEEDED(device->CreateRenderTargetView(texture.Get(), nullptr, &target)));
            return target;
        }
    };
}

TEST_CASE("UI restores pixel UAVs alongside sparse render targets and at the last supported slot") {
    Graphics graphics;
    auto originalTarget = graphics.Target(), uiTarget = graphics.Target();
    auto texture = graphics.Texture(D3D11_BIND_UNORDERED_ACCESS);
    ComPtr<ID3D11UnorderedAccessView> unordered;
    REQUIRE(SUCCEEDED(graphics.device->CreateUnorderedAccessView(texture.Get(), nullptr, &unordered)));
    const UINT lastSlot = graphics.device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_1 ? 63u : 7u;
    for (const UINT targetCount : {0u, 1u, 3u}) for (const UINT slot : {targetCount, lastSlot}) {
        std::array<ID3D11RenderTargetView*, 3> targets{};
        if (targetCount) targets[targetCount - 1] = originalTarget.Get();
        auto* view = unordered.Get();
        graphics.context->OMSetRenderTargetsAndUnorderedAccessViews(targetCount, targets.data(), nullptr,
            slot, 1, &view, nullptr);
        {
            b21ui::client::D3D11State saved(graphics.context.Get());
            auto* ui = uiTarget.Get();
            graphics.context->OMSetRenderTargets(1, &ui, nullptr);
        }
        ComPtr<ID3D11UnorderedAccessView> restored;
        graphics.context->OMGetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr,
            slot, 1, &restored);
        CHECK(restored.Get() == unordered.Get());
        std::array<ID3D11RenderTargetView*, 3> restoredTargets{};
        graphics.context->OMGetRenderTargets(3, restoredTargets.data(), nullptr);
        CHECK(restoredTargets == targets);
        for (auto* target : restoredTargets) if (target) target->Release();
        graphics.context->ClearState();
    }
}

TEST_CASE("UI drawing restores the graphics mod compute shader") {
    Graphics graphics;
    constexpr auto source = "[numthreads(1,1,1)] void main(uint3 id : SV_DispatchThreadID) {}";
    ComPtr<ID3DBlob> bytecode;
    REQUIRE(SUCCEEDED(D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "main", "cs_5_0", 0, 0, &bytecode, nullptr)));
    ComPtr<ID3D11ComputeShader> shader;
    REQUIRE(SUCCEEDED(graphics.device->CreateComputeShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, &shader)));
    auto* previous = ImGui::GetCurrentContext();
    auto* imgui = ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().DisplaySize = {64, 64};
    REQUIRE(ImGui_ImplDX11_Init(graphics.device.Get(), graphics.context.Get()));
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();
    ImGui::GetBackgroundDrawList()->AddRectFilled({0, 0}, {32, 32}, IM_COL32_WHITE);
    ImGui::Render();
    auto target = graphics.Target();
    graphics.context->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
    graphics.context->CSSetShader(shader.Get(), nullptr, 0);
    {
        b21ui::client::D3D11State saved(graphics.context.Get());
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
    ComPtr<ID3D11ComputeShader> restored;
    graphics.context->CSGetShader(&restored, nullptr, nullptr);
    CHECK(restored.Get() == shader.Get());
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext(imgui);
    ImGui::SetCurrentContext(previous);
}
