#include "b21ui/Common.h"

#include <imgui_impl_dx11.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

namespace b21ui::common {
    namespace {
        using Microsoft::WRL::ComPtr;

        // CSS of the web map's pip mode: the image gets grayscale(1) contrast(1.18) brightness(0.52),
        // then a 31% screen-blended layer: solid shadow under radial-gradient(circle at 50% 42%,
        // transparent 36%, shadow 78%, dim 100%) sized to the farthest corner of the image.
        constexpr char kPipBoyPs[] = R"(
Texture2D t : register(t0);
SamplerState s : register(s0);
cbuffer C : register(b0) { float4 shadow; float4 dim; float2 size; };
struct PS { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };
float4 main(PS i) : SV_Target {
    float4 c = t.Sample(s, i.uv);
    float g = dot(c.rgb, float3(0.2126, 0.7152, 0.0722));
    g = saturate(((g - 0.5) * 1.18 + 0.5) * 0.52);
    float2 p = i.uv * size;
    float2 center = float2(0.5, 0.42) * size;
    float radius = length(max(center, size - center));
    float r = length(p - center) / radius;
    float3 layer = lerp(shadow.rgb, dim.rgb, saturate((r - 0.78) / 0.22));
    float3 screen = 1.0 - (1.0 - g) * (1.0 - layer);
    return float4(lerp(float3(g, g, g), screen, 0.31), c.a * i.col.a);
}
)";

        // A CSS filter chain: each step is a colour matrix, clamped before the next.
        constexpr int kMaxFilterSteps = 8;
        constexpr char kFilterPs[] = R"(
Texture2D t : register(t0);
SamplerState s : register(s0);
cbuffer C : register(b0) { float4 rows[24]; uint count; };
struct PS { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };
float4 main(PS i) : SV_Target {
    float4 c = t.Sample(s, i.uv);
    float3 v = c.rgb;
    for (uint k = 0; k < count; ++k) {
        float4 x = float4(v, 1.0);
        v = saturate(float3(dot(rows[k * 3], x), dot(rows[k * 3 + 1], x), dot(rows[k * 3 + 2], x)));
    }
    return float4(v, c.a * i.col.a);
}
)";

        constexpr char kColorizePs[] = R"(
Texture2D t : register(t0);
SamplerState s : register(s0);
cbuffer C : register(b0) { float4 shadows; float4 midtones; float4 highlights; };
struct PS { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };
float4 main(PS i) : SV_Target {
    float4 c = t.Sample(s, i.uv);
    float lum = dot(c.rgb, float3(0.2126, 0.7152, 0.0722));
    float3 mapped = lum < 0.4 ? lerp(shadows.rgb, midtones.rgb, lum / 0.4) :
                              lerp(midtones.rgb, highlights.rgb, (lum - 0.4) / 0.6);
    float chroma = max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b));
    return float4(lerp(mapped, c.rgb, smoothstep(0.03, 0.18, chroma)), c.a * i.col.a);
}
)";

        // The CRT pass reads a copy of the render target (t1) instead of an ImGui texture.
        // Modest contrast keeps small UI text readable; glass and refresh effects have separate switches.
        constexpr char kCrtPs[] = R"(
Texture2D scene : register(t1);
SamplerState clampSampler : register(s1);
cbuffer C : register(b0) {
    float2 targetSize; float2 rectMin; float2 rectMax; float time; float curve;
    float glitchY; float glitchH; float glitchShift; float glitch; float screen; float brightness; float flicker;
};
struct PS { float4 pos : SV_POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };
float hash(float2 p) { return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453); }
float3 tap(float2 uv) { return scene.Sample(clampSampler, (rectMin + uv * (rectMax - rectMin)) / targetSize).rgb; }
float4 main(PS i) : SV_Target {
    float2 rs = rectMax - rectMin;
    float2 uv = (i.pos.xy - rectMin) / rs;
    float2 cc = uv - 0.5;
    float edge = 1.0;
    if (curve > 0.0) {
        // Curved glass: the middle of each edge stays put, the corners bend out of view.
        uv = cc * (1.0 + curve * dot(cc, cc)) / (1.0 + curve * 0.25) + 0.5;
        edge = smoothstep(0.0, 0.004, uv.x) * smoothstep(0.0, 0.004, 1.0 - uv.x) *
               smoothstep(0.0, 0.004, uv.y) * smoothstep(0.0, 0.004, 1.0 - uv.y);
        if (edge <= 0.0) return float4(0, 0, 0, 1);
    }
    float signal = brightness;
    if (flicker > 0.0) {
        float distance = abs(uv.y - frac(time / 8.0));
        distance = min(distance, 1.0 - distance);
        float refreshBand = 1.0 - smoothstep(0.0, 0.18, distance);
        signal *= 1.0 - 0.035 * refreshBand;
    }
    if (screen <= 0.0) return float4(saturate(tap(uv) * edge * signal), 1.0);

    // Now and then a thin band of lines slips sideways for a moment.
    float band = step(glitchY, uv.y) * step(uv.y, glitchY + glitchH) * glitch;
    float2 p = float2(uv.x + band * glitchShift, uv.y);

    // A pixel of convergence error, a faint composite smear and phosphor glow.
    float px = 1.0 / rs.x, py = 1.0 / rs.y;
    float2 fringe = float2(0.6 * px, 0.0);
    float3 col = float3(tap(p + fringe).r, tap(p).g, tap(p - fringe).b);
    float3 smear = tap(p - float2(2.0 * px, 0)) * 0.6 + tap(p - float2(4.0 * px, 0)) * 0.4;
    col = lerp(col, max(col, smear), 0.1);
    float3 glow = tap(p + float2(3 * px, 3 * py)) + tap(p + float2(-3 * px, 3 * py)) +
                  tap(p + float2(3 * px, -3 * py)) + tap(p + float2(-3 * px, -3 * py));
    glow *= 0.25;
    col += glow * glow * 0.25;

    // Scanlines about 360 lines tall at any resolution; bright rows bloom over the dark gap.
    float period = max(2.0, 3.0 * targetSize.y / 1080.0);
    float lum = dot(col, float3(0.299, 0.587, 0.114));
    float scan = 0.5 + 0.5 * sin((p.y * rs.y) * 6.2831853 / period);
    col *= 1.0 - 0.2 * (1.0 - scan) * (1.0 - 0.6 * saturate(lum));

    // Aperture grille: faint vertical R, G, B phosphor stripes.
    float stripe = fmod(floor(i.pos.x / max(1.0, targetSize.y / 1080.0)), 3.0);
    float3 mask = stripe < 1.0 ? float3(1.18, 0.86, 0.86) : stripe < 2.0 ? float3(0.86, 1.18, 0.86) : float3(0.86, 0.86, 1.18);
    col *= lerp(float3(1, 1, 1), mask, 0.18);

    // Fine grain, and a little darkening toward the corners.
    col += (hash(i.pos.xy * 0.73 + frac(time * 7.13) * 311.0) - 0.5) * 0.03;
    float2 v = curve > 0.0 ? uv : float2(0.5 + cc.x, 0.5 + cc.y);
    col *= pow(saturate(16.0 * v.x * v.y * (1.0 - v.x) * (1.0 - v.y)), 0.1) * edge * 1.06 * signal;
    return float4(saturate(col), 1.0);
}
)";

        struct CrtConstants {
            float targetW, targetH, minX, minY, maxX, maxY, time, curve;
            float glitchY, glitchH, glitchShift, glitch;
            float screen, brightness, flicker, pad;
        };

        struct PipBoyConstants {
            ImVec4 shadow, dim;
            float width, height, pad[2];
        };

        struct FilterConstants {
            float rows[kMaxFilterSteps * 3][4];
            std::uint32_t count, pad[3];
        };

        struct ColorizeConstants { ImVec4 shadows, midtones, highlights; };

        struct Shader {
            ComPtr<ID3D11PixelShader> ps;
            ComPtr<ID3D11Buffer> constants;
            bool failed{};
        };
        struct DeviceObjects {
            ID3D11Device* device{};
            Shader pipBoy, filter, colorize, crt;
            // The CRT pass's copy of the render target, and the clamp sampler it reads it with.
            ComPtr<ID3D11Texture2D> sceneCopy;
            ComPtr<ID3D11ShaderResourceView> sceneView;
            ComPtr<ID3D11SamplerState> sceneSampler;
            ComPtr<ID3D11BlendState> noWrite;
            D3D11_TEXTURE2D_DESC sceneDesc{};
            DXGI_FORMAT sceneViewFormat{};
        };
        DeviceObjects objects;   // render thread only

        Shader* Ensure(ID3D11Device* device, Shader DeviceObjects::*which, const char* source, std::size_t length,
                       const char* name, UINT constantsSize) {
            if (objects.device != device) objects = {device};
            auto& shader = objects.*which;
            if (shader.ps) return &shader;
            if (shader.failed || !device) return nullptr;
            shader.failed = true;
            ComPtr<ID3DBlob> code, errors;
            if (FAILED(::D3DCompile(source, length, name, nullptr, nullptr, "main", "ps_4_0", 0, 0, &code, &errors))) return nullptr;
            if (FAILED(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader.ps))) return nullptr;
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = constantsSize;
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if (FAILED(device->CreateBuffer(&desc, nullptr, &shader.constants))) {
                shader.ps.Reset();
                return nullptr;
            }
            shader.failed = false;
            return &shader;
        }

        void Bind(const ImDrawCmd* cmd, Shader* shader, std::size_t constantsSize) {
            const auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
            if (!state || !shader) return;
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(state->DeviceContext->Map(shader->constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
            std::memcpy(mapped.pData, cmd->UserCallbackData, constantsSize);
            state->DeviceContext->Unmap(shader->constants.Get(), 0);
            state->DeviceContext->PSSetShader(shader->ps.Get(), nullptr, 0);
            ID3D11Buffer* buffers[] = {shader->constants.Get()};
            state->DeviceContext->PSSetConstantBuffers(0, 1, buffers);
        }

        ID3D11Device* RenderDevice() {
            const auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
            return state ? state->Device : nullptr;
        }

        void BindPipBoy(const ImDrawList*, const ImDrawCmd* cmd) {
            Bind(cmd, Ensure(RenderDevice(), &DeviceObjects::pipBoy, kPipBoyPs, sizeof(kPipBoyPs) - 1, "b21ui_pipboy", sizeof(PipBoyConstants)),
                 sizeof(PipBoyConstants));
        }

        void BindFilter(const ImDrawList*, const ImDrawCmd* cmd) {
            Bind(cmd, Ensure(RenderDevice(), &DeviceObjects::filter, kFilterPs, sizeof(kFilterPs) - 1, "b21ui_filter", sizeof(FilterConstants)),
                 sizeof(FilterConstants));
        }

        void BindColorized(const ImDrawList*, const ImDrawCmd* cmd) {
            Bind(cmd, Ensure(RenderDevice(), &DeviceObjects::colorize, kColorizePs, sizeof(kColorizePs) - 1,
                             "b21ui_colorize", sizeof(ColorizeConstants)), sizeof(ColorizeConstants));
        }

        // Copies the bound render target into objects.sceneCopy (resolving MSAA); false when there is none.
        bool CopyRenderTarget(ID3D11Device* device, ID3D11DeviceContext* context, UINT& width, UINT& height) {
            ComPtr<ID3D11RenderTargetView> rtv;
            context->OMGetRenderTargets(1, &rtv, nullptr);
            if (!rtv) return false;
            ComPtr<ID3D11Resource> resource;
            rtv->GetResource(&resource);
            ComPtr<ID3D11Texture2D> target;
            if (!resource || FAILED(resource.As(&target))) return false;
            D3D11_TEXTURE2D_DESC desc{};
            target->GetDesc(&desc);
            D3D11_RENDER_TARGET_VIEW_DESC view{};
            rtv->GetDesc(&view);
            const DXGI_FORMAT viewFormat = view.Format != DXGI_FORMAT_UNKNOWN ? view.Format : desc.Format;
            auto& o = objects;
            if (!o.sceneCopy || o.sceneDesc.Width != desc.Width || o.sceneDesc.Height != desc.Height ||
                o.sceneDesc.Format != desc.Format || o.sceneViewFormat != viewFormat) {
                o.sceneCopy.Reset();
                o.sceneView.Reset();
                D3D11_TEXTURE2D_DESC copy{};
                copy.Width = desc.Width;
                copy.Height = desc.Height;
                copy.MipLevels = 1;
                copy.ArraySize = 1;
                copy.Format = desc.Format;
                copy.SampleDesc.Count = 1;
                copy.Usage = D3D11_USAGE_DEFAULT;
                copy.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                if (FAILED(device->CreateTexture2D(&copy, nullptr, &o.sceneCopy))) return false;
                D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
                srv.Format = viewFormat;
                srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srv.Texture2D.MipLevels = 1;
                if (FAILED(device->CreateShaderResourceView(o.sceneCopy.Get(), &srv, &o.sceneView))) {
                    o.sceneCopy.Reset();
                    return false;
                }
                o.sceneDesc = desc;
                o.sceneViewFormat = viewFormat;
            }
            if (!o.sceneSampler) {
                D3D11_SAMPLER_DESC sampler{};
                sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
                sampler.MaxLOD = D3D11_FLOAT32_MAX;
                if (FAILED(device->CreateSamplerState(&sampler, &o.sceneSampler))) return false;
            }
            if (desc.SampleDesc.Count > 1)
                context->ResolveSubresource(o.sceneCopy.Get(), 0, target.Get(), view.Texture2D.MipSlice, viewFormat);
            else
                context->CopySubresourceRegion(o.sceneCopy.Get(), 0, 0, 0, 0, target.Get(), view.Texture2D.MipSlice, nullptr);
            width = desc.Width;
            height = desc.Height;
            return true;
        }

        void BindCrt(const ImDrawList*, const ImDrawCmd* cmd) {
            const auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
            if (!state) return;
            auto* shader = Ensure(state->Device, &DeviceObjects::crt, kCrtPs, sizeof(kCrtPs) - 1, "b21ui_crt", sizeof(CrtConstants));
            UINT width{}, height{};
            if (!shader || !CopyRenderTarget(state->Device, state->DeviceContext, width, height)) {
                // No CRT this frame: the rect after this callback must not paint over the screen.
                if (!objects.noWrite) {
                    D3D11_BLEND_DESC blend{};
                    blend.RenderTarget[0].SrcBlend = blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
                    blend.RenderTarget[0].DestBlend = blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
                    blend.RenderTarget[0].BlendOp = blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
                    state->Device->CreateBlendState(&blend, &objects.noWrite);
                }
                const float factor[4]{};
                state->DeviceContext->OMSetBlendState(objects.noWrite.Get(), factor, 0xFFFFFFFF);
                return;
            }
            auto constants = *static_cast<const CrtConstants*>(cmd->UserCallbackData);
            constants.targetW = static_cast<float>(width);
            constants.targetH = static_cast<float>(height);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(state->DeviceContext->Map(shader->constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
            std::memcpy(mapped.pData, &constants, sizeof(constants));
            state->DeviceContext->Unmap(shader->constants.Get(), 0);
            state->DeviceContext->PSSetShader(shader->ps.Get(), nullptr, 0);
            ID3D11Buffer* buffers[] = {shader->constants.Get()};
            state->DeviceContext->PSSetConstantBuffers(0, 1, buffers);
            ID3D11ShaderResourceView* views[] = {objects.sceneView.Get()};
            state->DeviceContext->PSSetShaderResources(1, 1, views);
            ID3D11SamplerState* samplers[] = {objects.sceneSampler.Get()};
            state->DeviceContext->PSSetSamplers(1, 1, samplers);
        }

        // The copy must not stay bound as an input when the next frame copies into it.
        void UnbindCrt(const ImDrawList*, const ImDrawCmd*) {
            const auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
            if (!state) return;
            ID3D11ShaderResourceView* none[] = {nullptr};
            state->DeviceContext->PSSetShaderResources(1, 1, none);
        }

        // Line slips hold still until they end. Refresh brightness uses elapsed time, independent of frame rate.
        struct CrtSignal {
            std::mt19937 rng{std::random_device{}()};
            double nextSlip = 6.0, slipEnd = 0.0;
            float slipY = 0, slipH = 0, slipShift = 0;

            float Uniform(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }

            void Update(double now, bool flicker, CrtConstants& out) {
                if (now >= nextSlip) {
                    slipEnd = now + Uniform(0.08F, 0.18F);
                    slipY = Uniform(0.0F, 0.95F);
                    slipH = Uniform(0.005F, 0.03F);
                    slipShift = (Uniform(0, 1) < 0.5F ? -1.0F : 1.0F) * Uniform(0.003F, 0.008F);
                    nextSlip = slipEnd + Uniform(6.0F, 16.0F);
                }
                out.glitch = now < slipEnd ? 1.0F : 0.0F;
                out.glitchY = slipY;
                out.glitchH = slipH;
                out.glitchShift = slipShift;
                out.brightness = 1.0F;
                out.flicker = flicker ? 1.0F : 0.0F;
                if (flicker) out.brightness = static_cast<float>(0.995 + 0.0035 * std::sin(now * 6.283185307 * 7.0) +
                                                               0.0015 * std::sin(now * 6.283185307 * 13.0));
            }
        } crtSignal;   // UI thread only

        // If a shader cannot be built its callback binds nothing and the image draws unfiltered.
        void AddShadedImage(ImDrawList* list, const Texture& texture, ImVec2 min, ImVec2 max, float alpha, ImDrawCallback bind,
                            const void* constants, std::size_t size) {
            list->AddCallback(bind, const_cast<void*>(constants), size);
            list->AddImage(texture.Id(), min, max, {0, 0}, {1, 1}, IM_COL32(255, 255, 255, static_cast<int>(alpha * 255.0F)));
            list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
        }
    }

    void DrawPipBoyImage(ImDrawList* list, const Texture& texture, ImVec2 min, ImVec2 max, ImU32 shadow, ImU32 dim,
                         float alpha) {
        if (!list || !texture) return;
        const PipBoyConstants constants{ImGui::ColorConvertU32ToFloat4(shadow), ImGui::ColorConvertU32ToFloat4(dim),
                                        static_cast<float>(texture.width), static_cast<float>(texture.height), {}};
        AddShadedImage(list, texture, min, max, alpha, &BindPipBoy, &constants, sizeof(constants));
    }

    void DrawFilteredImage(ImDrawList* list, const Texture& texture, ImVec2 min, ImVec2 max,
                           std::span<const css::ColorMatrix> steps, float alpha) {
        if (!list || !texture) return;
        FilterConstants constants{};
        constants.count = static_cast<std::uint32_t>(std::min<std::size_t>(steps.size(), kMaxFilterSteps));
        for (std::uint32_t k = 0; k < constants.count; ++k)
            for (int r = 0; r < 3; ++r) std::memcpy(constants.rows[k * 3 + r], steps[k].rows[static_cast<std::size_t>(r)].data(), sizeof(float) * 4);
        AddShadedImage(list, texture, min, max, alpha, &BindFilter, &constants, sizeof(constants));
    }

    void DrawColorizedImage(ImDrawList* list, const Texture& texture, ImVec2 min, ImVec2 max, float alpha) {
        if (!list || !texture) return;
        const ColorizeConstants constants{{0.025F, 0.04F, 0.045F, 1}, {0.34F, 0.40F, 0.29F, 1}, {0.91F, 0.85F, 0.68F, 1}};
        AddShadedImage(list, texture, min, max, alpha, &BindColorized, &constants, sizeof(constants));
    }

    void DrawCrtEffect(ImDrawList* list, ImVec2 min, ImVec2 max, const CrtOptions& options) {
        if (!options.screen && !options.curved && !options.flicker) return;
        if (!list || max.x <= min.x || max.y <= min.y) return;
        const double now = ImGui::GetTime();
        CrtConstants constants{};
        constants.minX = min.x;
        constants.minY = min.y;
        constants.maxX = max.x;
        constants.maxY = max.y;
        constants.time = static_cast<float>(std::fmod(now, 1000.0));
        constants.curve = options.curved ? 0.11F : 0.0F;
        constants.screen = options.screen ? 1.0F : 0.0F;
        crtSignal.Update(now, options.flicker, constants);
        list->AddCallback(&BindCrt, &constants, sizeof(constants));
        list->AddRectFilled(min, max, IM_COL32_WHITE);
        list->AddCallback(&UnbindCrt, nullptr);
        list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    }

    void ReleaseDeviceObjects() { objects = {}; }
}
