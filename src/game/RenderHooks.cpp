#include <F4SE/F4SE.h>
#include <RE/Fallout.h>

#include "game/RenderHooks.h"
#include "game/Host.h"
#include "client/D3D11State.h"

#include <spdlog/spdlog.h>

#include <d3d11.h>
#include <dxgi.h>

#include <chrono>

namespace b21ui::game::RenderHooks {
    namespace {
        using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
        using ResizeFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

        PresentFn originalPresent{};
        ResizeFn originalResize{};
        ID3D11Device* device{};
        ID3D11DeviceContext* context{};
        HWND window{};
        ID3D11RenderTargetView* backbuffer{};
        UINT backbufferWidth{}, backbufferHeight{};
        bool foreground{true};
        auto lastFrame = std::chrono::steady_clock::now();

        // Chains whatever pointer is already in the slot (ENB, ReShade, Community Shaders) and returns it.
        template <class Fn>
        Fn SwapSlot(void** vtable, std::size_t slot, void* replacement) {
            DWORD old{};
            ::VirtualProtect(&vtable[slot], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
            auto* original = ::InterlockedExchangePointer(&vtable[slot], replacement);
            ::VirtualProtect(&vtable[slot], sizeof(void*), old, &old);
            return reinterpret_cast<Fn>(original);
        }

        void ReleaseBackbuffer() {
            if (backbuffer) backbuffer->Release();
            backbuffer = nullptr;
        }

        bool EnsureBackbuffer(IDXGISwapChain* chain) {
            if (backbuffer) return true;
            ID3D11Texture2D* texture{};
            if (FAILED(chain->GetBuffer(0, IID_PPV_ARGS(&texture))) || !texture) return false;
            D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);
            backbufferWidth = desc.Width;
            backbufferHeight = desc.Height;
            device->CreateRenderTargetView(texture, nullptr, &backbuffer);
            texture->Release();
            return backbuffer != nullptr;
        }

        void BindBackbuffer() {
            context->OMSetRenderTargets(1, &backbuffer, nullptr);
        }

        HRESULT STDMETHODCALLTYPE HookedPresent(IDXGISwapChain* chain, UINT sync, UINT flags) {
            const bool fg = ::GetForegroundWindow() == window;
            if (fg != foreground) {
                foreground = fg;
                Host::Get().OnWindowFocus(fg);
            }
            const auto now = std::chrono::steady_clock::now();
            const float dt = std::chrono::duration<float>(now - lastFrame).count();
            lastFrame = now;
            if (EnsureBackbuffer(chain)) {
                client::D3D11State saved(context);
                BindBackbuffer();
                const auto w = static_cast<float>(backbufferWidth), h = static_cast<float>(backbufferHeight);
                auto& host = Host::Get();
                host.RenderClients(false, device, context, w, h, dt, 0);
                // M0: the modal draws in Present, last, with the B21UI cursor on top of everything.
                host.RenderClients(true, device, context, w, h, dt, B21UI_FRAME_DRAW_CURSOR);
            }
            const auto result = originalPresent(chain, sync, flags);
            if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET) {
                ReleaseBackbuffer();
                Host::Get().DeviceLost();
            }
            return result;
        }

        HRESULT STDMETHODCALLTYPE HookedResize(IDXGISwapChain* chain, UINT count, UINT width, UINT height, DXGI_FORMAT format,
                                               UINT flags) {
            ReleaseBackbuffer();
            return originalResize(chain, count, width, height, format, flags);
        }
    }

    void Install() {
        auto* rendererWindow = RE::BSGraphics::GetCurrentRendererWindow();
        auto* renderer = RE::BSGraphics::GetRendererData();
        IDXGISwapChain* swapChain{};
        if (rendererWindow) {
            swapChain = reinterpret_cast<IDXGISwapChain*>(rendererWindow->swapChain);
            window = reinterpret_cast<HWND>(rendererWindow->hwnd);
        }
        if (!swapChain && renderer) {
            swapChain = reinterpret_cast<IDXGISwapChain*>(renderer->renderWindow[0].swapChain);
            window = reinterpret_cast<HWND>(renderer->renderWindow[0].hwnd);
        }
        if (renderer) {
            device = reinterpret_cast<ID3D11Device*>(renderer->device);
            context = reinterpret_cast<ID3D11DeviceContext*>(renderer->context);
        }
        if (!swapChain || !device || !context) {
            spdlog::error("B21UI: renderer not available; UI disabled");
            Host::Get().SetRenderAvailable(false);
            return;
        }
        auto** vtable = *reinterpret_cast<void***>(swapChain);
        originalPresent = SwapSlot<PresentFn>(vtable, 8, reinterpret_cast<void*>(&HookedPresent));
        originalResize = SwapSlot<ResizeFn>(vtable, 13, reinterpret_cast<void*>(&HookedResize));
        Host::Get().SetRenderAvailable(true);
        spdlog::info("B21UI: Present/ResizeBuffers hooked");
    }

    HWND Window() { return window; }
}
