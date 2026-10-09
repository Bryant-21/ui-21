#include "preview/PreviewHost.h"
#include "b21ui/B21UI.h"
#include "b21ui/Kit.h"
#include "b21ui/Paths.h"
#include "b21ui/Windows.h"
#include "b21ui/Settings.h"
#include "client/ClientRuntime.h"
#include "core/InputTranslate.h"
#include "core/SettingsRegistry.h"

#include <Windows.h>
#include <Xinput.h>
#include <d3d11.h>
#include <dxgi.h>
#include <stb_image_write.h>

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace b21ui::preview {
    namespace {
        std::vector<std::pair<Client*, std::unique_ptr<client::ClientRuntime>>>& Clients() {
            static std::vector<std::pair<Client*, std::unique_ptr<client::ClientRuntime>>> clients;
            return clients;
        }
        std::set<const Client*> closed;
        core::SettingsRegistry settings;
        std::vector<B21UI_Event> events;
        bool gamepad{};
        bool forceGamepad{};
        std::uint32_t hudColor = 0xFF12FF15;
        std::vector<std::uint32_t> padScript;
        // --click / --move / --text / --key steps, run in command-line order, one every 12 frames after the pad script.
        struct ScriptStep {
            enum class Kind { Click, Text, Key } kind;
            float x = 0, y = 0;
            int count = 1;
            std::string text;
            std::uint32_t vk = 0;
        };
        std::vector<ScriptStep> script;

        std::uint32_t KeyToken(const std::string& name) {
            static const std::pair<const char*, std::uint32_t> names[]{
                {"Enter", VK_RETURN}, {"Esc", VK_ESCAPE}, {"Up", VK_UP},   {"Down", VK_DOWN},
                {"Left", VK_LEFT},    {"Right", VK_RIGHT}, {"Tab", VK_TAB}, {"Back", VK_BACK}};
            for (const auto& [n, vk] : names)
                if (name == n) return vk;
            return name.size() == 1 ? static_cast<std::uint32_t>(std::toupper(static_cast<unsigned char>(name[0]))) : 0;
        }
        constexpr std::uint32_t kStickToken = 0x100;

        std::optional<std::uint32_t> PadToken(const std::string& token) {
            static const std::pair<const char*, std::uint32_t> names[]{
                {"A", B21UI_PAD_A}, {"B", B21UI_PAD_B}, {"X", B21UI_PAD_X}, {"Y", B21UI_PAD_Y},
                {"LB", B21UI_PAD_LB}, {"RB", B21UI_PAD_RB}, {"LT", B21UI_PAD_LT}, {"RT", B21UI_PAD_RT},
                {"U", B21UI_PAD_DPAD_UP}, {"D", B21UI_PAD_DPAD_DOWN}, {"L", B21UI_PAD_DPAD_LEFT}, {"R", B21UI_PAD_DPAD_RIGHT},
                {"BK", B21UI_PAD_BACK}, {"ST", B21UI_PAD_START}};
            for (const auto& [name, code] : names)
                if (token == name) return code;
            // Stick holds for one script slot: SL/SR/SU/SD push the right stick, CL/CR/CU/CD the left.
            if (token.size() == 2 && (token[0] == 'S' || token[0] == 'C') && std::string("LRUD").find(token[1]) != std::string::npos)
                return kStickToken + (token[0] == 'C' ? 4u : 0u) + static_cast<std::uint32_t>(std::string("LRUD").find(token[1]));
            return std::nullopt;
        }
        struct { float x{}, y{}; } cursor;
        HWND hwnd{};
        ID3D11Device* device{};
        ID3D11DeviceContext* context{};
        IDXGISwapChain* swapChain{};
        ID3D11RenderTargetView* rtv{};

        void CreateTarget() {
            ID3D11Texture2D* back{};
            if (SUCCEEDED(swapChain->GetBuffer(0, IID_PPV_ARGS(&back)))) {
                device->CreateRenderTargetView(back, nullptr, &rtv);
                back->Release();
            }
        }
        void ReleaseTarget() {
            if (rtv) rtv->Release();
            rtv = nullptr;
        }

        void Push(std::uint32_t type, std::uint32_t code, bool down, float x = 0.0F, float y = 0.0F) {
            events.push_back({type, code, down ? 1u : 0u, x, y});
        }

        LRESULT CALLBACK WndProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
            switch (msg) {
            case WM_LBUTTONDOWN: case WM_LBUTTONUP: Push(B21UI_EV_MOUSE_BUTTON, 0, msg == WM_LBUTTONDOWN); gamepad = false; return 0;
            case WM_RBUTTONDOWN: case WM_RBUTTONUP: Push(B21UI_EV_MOUSE_BUTTON, 1, msg == WM_RBUTTONDOWN); gamepad = false; return 0;
            case WM_MBUTTONDOWN: case WM_MBUTTONUP: Push(B21UI_EV_MOUSE_BUTTON, 2, msg == WM_MBUTTONDOWN); gamepad = false; return 0;
            case WM_MOUSEWHEEL:
                Push(B21UI_EV_MOUSE_WHEEL, 0, true, 0.0F, static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
                return 0;
            case WM_KEYDOWN: case WM_SYSKEYDOWN:
                if (lParam & (1 << 30)) return 0;
                Push(B21UI_EV_KEY, static_cast<std::uint32_t>(wParam), true);
                gamepad = false;
                return 0;
            case WM_KEYUP: case WM_SYSKEYUP: Push(B21UI_EV_KEY, static_cast<std::uint32_t>(wParam), false); return 0;
            case WM_CHAR:
                if (wParam >= 0x20 && wParam != 0x7F) Push(B21UI_EV_CHAR, static_cast<std::uint32_t>(wParam), true);
                return 0;
            case WM_MOUSEMOVE: {
                const float x = static_cast<float>(static_cast<short>(LOWORD(lParam)));
                const float y = static_cast<float>(static_cast<short>(HIWORD(lParam)));
                if (!forceGamepad && (x != cursor.x || y != cursor.y)) gamepad = false;
                cursor = {x, y};
                return 0;
            }
            case WM_SETCURSOR:
                if (LOWORD(lParam) == HTCLIENT) { ::SetCursor(nullptr); return TRUE; }
                break;
            case WM_SIZE:
                if (swapChain && wParam != SIZE_MINIMIZED) {
                    ReleaseTarget();
                    swapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
                    CreateTarget();
                }
                return 0;
            case WM_ACTIVATE: Push(B21UI_EV_FOCUS, 0, LOWORD(wParam) != WA_INACTIVE); return 0;
            case WM_DESTROY: ::PostQuitMessage(0); return 0;
            default: break;
            }
            return ::DefWindowProcW(window, msg, wParam, lParam);
        }

        void PollPad() {
            static WORD lastButtons{};
            static bool lastLt{}, lastRt{};
            XINPUT_STATE state{};
            if (::XInputGetState(0, &state) != ERROR_SUCCESS) return;
            const auto& pad = state.Gamepad;
            for (WORD mask = 1; mask != 0; mask <<= 1) {
                if (((pad.wButtons ^ lastButtons) & mask) == 0) continue;
                if (const auto button = core::PadButtonFromGameCode(mask)) {
                    const bool down = (pad.wButtons & mask) != 0;
                    Push(B21UI_EV_PAD_BUTTON, *button, down, down ? 1.0F : 0.0F);
                    if (down) gamepad = true;
                }
            }
            lastButtons = pad.wButtons;
            const bool lt = pad.bLeftTrigger > 128, rt = pad.bRightTrigger > 128;
            if (lt != lastLt) { Push(B21UI_EV_PAD_BUTTON, B21UI_PAD_LT, lt, pad.bLeftTrigger / 255.0F); gamepad |= lt; }
            if (rt != lastRt) { Push(B21UI_EV_PAD_BUTTON, B21UI_PAD_RT, rt, pad.bRightTrigger / 255.0F); gamepad |= rt; }
            lastLt = lt;
            lastRt = rt;
            const auto axis = [](SHORT v) { return v < 0 ? v / 32768.0F : v / 32767.0F; };
            const float lx = axis(pad.sThumbLX), ly = axis(pad.sThumbLY), rx = axis(pad.sThumbRX), ry = axis(pad.sThumbRY);
            Push(B21UI_EV_PAD_STICK, B21UI_STICK_LEFT, true, lx, ly);
            Push(B21UI_EV_PAD_STICK, B21UI_STICK_RIGHT, true, rx, ry);
            if (std::abs(lx) > 0.2F || std::abs(ly) > 0.2F || std::abs(rx) > 0.2F || std::abs(ry) > 0.2F) gamepad = true;
        }

        bool Screenshot(const std::string& file) {
            ID3D11Texture2D* back{};
            if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
            D3D11_TEXTURE2D_DESC desc{};
            back->GetDesc(&desc);
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.MiscFlags = 0;
            ID3D11Texture2D* staging{};
            const bool ok = SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &staging));
            if (ok) context->CopyResource(staging, back);
            back->Release();
            if (!ok) return false;
            D3D11_MAPPED_SUBRESOURCE mapped{};
            bool written = false;
            if (SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
                std::vector<std::uint8_t> rgba(static_cast<std::size_t>(desc.Width) * desc.Height * 4);
                const bool bgra = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM;
                for (UINT y = 0; y < desc.Height; ++y) {
                    const auto* src = static_cast<const std::uint8_t*>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch;
                    auto* dst = rgba.data() + static_cast<std::size_t>(y) * desc.Width * 4;
                    for (UINT x = 0; x < desc.Width; ++x) {
                        dst[x * 4 + 0] = src[x * 4 + (bgra ? 2 : 0)];
                        dst[x * 4 + 1] = src[x * 4 + 1];
                        dst[x * 4 + 2] = src[x * 4 + (bgra ? 0 : 2)];
                        dst[x * 4 + 3] = 255;
                    }
                }
                context->Unmap(staging, 0);
                written = stbi_write_png(file.c_str(), static_cast<int>(desc.Width), static_cast<int>(desc.Height), 4,
                                         rgba.data(), static_cast<int>(desc.Width * 4)) != 0;
            }
            staging->Release();
            return written;
        }
    }

    void SetHudColor(float r, float g, float b) {
        const auto color = ImGui::ColorConvertFloat4ToU32({r, g, b, 1.0F});
        hudColor = 0xFF000000 | ((color >> IM_COL32_R_SHIFT & 0xFF) << 16) |
                   ((color >> IM_COL32_G_SHIFT & 0xFF) << 8) | (color >> IM_COL32_B_SHIFT & 0xFF);
    }

    int Run(int argc, char** argv, const std::filesystem::path& assetDir) {
        std::string screenshot;
        int frames = 0, width = 1600, height = 900;
        bool hidden = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
            else if (arg == "--hidden") hidden = true;
            else if (arg == "--hud-color" && i + 1 < argc) {
                float r{}, g{}, b{};
                if (sscanf_s(argv[++i], "%f,%f,%f", &r, &g, &b) == 3) SetHudColor(r, g, b);
            }
            else if (arg == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
            else if (arg == "--size" && i + 1 < argc) sscanf_s(argv[++i], "%dx%d", &width, &height);
            else if (arg == "--gamepad") gamepad = forceGamepad = true;
            else if (arg == "--pad" && i + 1 < argc) {
                // Scripted gamepad presses, e.g. --pad "D D R A": one token every 6 frames from frame 10.
                std::string token;
                for (const char* s = argv[++i];; ++s) {
                    if (*s && *s != ' ') { token += *s; continue; }
                    if (const auto code = PadToken(token)) padScript.push_back(*code);
                    token.clear();
                    if (!*s) break;
                }
                gamepad = forceGamepad = true;
            } else if (arg == "--click" && i + 1 < argc) {
                // --click x,y[,2]: a left click (or double click) there.
                ScriptStep step{ScriptStep::Kind::Click};
                sscanf_s(argv[++i], "%f,%f,%d", &step.x, &step.y, &step.count);
                script.push_back(step);
            } else if (arg == "--move" && i + 1 < argc) {
                // --move x,y: the pointer moves there without clicking.
                ScriptStep step{ScriptStep::Kind::Click};
                step.count = 0;
                sscanf_s(argv[++i], "%f,%f", &step.x, &step.y);
                script.push_back(step);
            } else if (arg == "--text" && i + 1 < argc) {
                script.push_back({ScriptStep::Kind::Text, 0, 0, 1, argv[++i]});
            } else if (arg == "--key" && i + 1 < argc) {
                // --key Enter|Esc|Up|Down|Left|Right|Tab|Back|<letter>
                ScriptStep step{ScriptStep::Kind::Key};
                step.vk = KeyToken(argv[++i]);
                script.push_back(step);
            }
        }
        if (!screenshot.empty() && frames <= 0) frames = 30;
        SetAssetDirOverride(assetDir);

        const WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, WndProc, 0, 0, ::GetModuleHandleW(nullptr), nullptr, nullptr, nullptr,
                             nullptr, L"B21UIPreview", nullptr};
        ::RegisterClassExW(&wc);
        RECT rect{0, 0, width, height};
        ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        hwnd = ::CreateWindowW(wc.lpszClassName, L"B21UI Preview", WS_OVERLAPPEDWINDOW, 100, 100, rect.right - rect.left,
                               rect.bottom - rect.top, nullptr, nullptr, wc.hInstance, nullptr);

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        if (FAILED(::D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                                   D3D11_SDK_VERSION, &sd, &swapChain, &device, nullptr, &context)))
            return 1;
        CreateTarget();
        if (!hidden) ::ShowWindow(hwnd, screenshot.empty() ? SW_SHOWDEFAULT : SW_SHOWNOACTIVATE);

        auto last = std::chrono::steady_clock::now();
        for (int frame = 1;; ++frame) {
            MSG msg;
            while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) goto done;
                ::TranslateMessage(&msg);
                ::DispatchMessageW(&msg);
            }
            PollPad();
            if (frame >= 10 && static_cast<std::size_t>((frame - 10) / 6) < padScript.size()) {
                const auto code = padScript[(frame - 10) / 6];
                const int phase = (frame - 10) % 6;
                if (code >= kStickToken) {
                    static constexpr float dx[]{-1, 1, 0, 0}, dy[]{0, 0, 1, -1};
                    const auto dir = (code - kStickToken) % 4;
                    const auto stick = code - kStickToken >= 4 ? B21UI_STICK_LEFT : B21UI_STICK_RIGHT;
                    if (phase == 0) Push(B21UI_EV_PAD_STICK, stick, true, dx[dir], dy[dir]);
                    if (phase == 5) Push(B21UI_EV_PAD_STICK, stick, true, 0.0F, 0.0F);
                } else if (phase < 2) {
                    Push(B21UI_EV_PAD_BUTTON, code, phase == 0, 1.0F);
                }
            }
            const int scriptStart = 10 + static_cast<int>(padScript.size()) * 6;
            if (frame >= scriptStart && static_cast<std::size_t>((frame - scriptStart) / 12) < script.size()) {
                const auto& step = script[static_cast<std::size_t>((frame - scriptStart) / 12)];
                const int phase = (frame - scriptStart) % 12;
                if (step.kind == ScriptStep::Kind::Click) {
                    // Arrive a frame before pressing, like a real pointer (overlap-allowed items need prior hover).
                    cursor = {step.x, step.y};
                    if (phase >= 1 && phase <= 2 * step.count) Push(B21UI_EV_MOUSE_BUTTON, 0, phase % 2 == 1);
                } else if (step.kind == ScriptStep::Kind::Text && phase == 0) {
                    for (const char c : step.text) Push(B21UI_EV_CHAR, static_cast<unsigned char>(c), true);
                } else if (step.kind == ScriptStep::Kind::Key && phase < 2) {
                    Push(B21UI_EV_KEY, step.vk, phase == 0);
                }
            }
            const auto now = std::chrono::steady_clock::now();
            // Scripted runs step a fixed 1/60 s so stick travel does not depend on the frame rate.
            const bool scripted = !padScript.empty() || !script.empty();
            const float dt = scripted ? 1.0F / 60.0F : std::chrono::duration<float>(now - last).count();
            last = now;
            RECT client{};
            ::GetClientRect(hwnd, &client);
            const float color[4]{0x31 / 255.0F, 0x39 / 255.0F, 0x3B / 255.0F, 1.0F};
            context->OMSetRenderTargets(1, &rtv, nullptr);
            context->ClearRenderTargetView(rtv, color);
            const B21UI_Frame f{sizeof(B21UI_Frame), B21UI_FRAME_DRAW_CURSOR,
                static_cast<float>(client.right - client.left), static_cast<float>(client.bottom - client.top), dt,
                cursor.x, cursor.y, gamepad ? static_cast<std::uint32_t>(B21UI_DEVICE_GAMEPAD) : static_cast<std::uint32_t>(B21UI_DEVICE_KEYBOARD_MOUSE),
                1u, device, context, events.data(), static_cast<std::uint32_t>(events.size()), hudColor};
            for (auto& [c, runtime] : Clients())
                if (!closed.contains(c)) runtime->Render(f);
            events.clear();
            swapChain->Present(1, 0);
            if (frames > 0 && frame >= frames) {
                const bool ok = screenshot.empty() || Screenshot(screenshot);
                ::DestroyWindow(hwnd);
                return ok ? 0 : 2;
            }
        }
    done:
        return 0;
    }
}

namespace b21ui {
    bool Register(Client& c, const ClientOptions& o) {
        if (o.settings && !o.modal) return false;
        for (const auto& [client, runtime] : preview::Clients())
            if (client == &c) return true;
        preview::Clients().emplace_back(&c, std::make_unique<client::ClientRuntime>(c, o.name ? o.name : "", o.padPointer,
                                                                                o.scaleWithResolution, o.settings));
        if (o.settings) {
            B21UI_ClientDesc desc{};
            desc.size = sizeof(desc);
            desc.name = o.name;
            desc.flags = B21UI_FLAG_SETTINGS;
            desc.settingsLabel = o.settingsLabel;
            desc.settingsIcon = o.settingsIcon;
            desc.settingsCategory = o.settingsCategory;
            preview::settings.Add(static_cast<B21UI_ClientId>(preview::Clients().size()), desc);
            preview::closed.insert(&c);
        }
        return true;
    }
    bool Open(Client& c) {
        for (std::size_t i = 0; i < preview::Clients().size(); ++i)
            if (preview::Clients()[i].first == &c && preview::settings.Contains(static_cast<B21UI_ClientId>(i + 1)))
                return OpenSettings(static_cast<B21UI_ClientId>(i + 1));
        preview::closed.erase(&c);
        c.OnFocusChanged(true);
        return true;
    }
    void Close(Client& c) {
        if (!preview::closed.insert(&c).second) return;
        for (const auto& [client, runtime] : preview::Clients())
            if (client == &c) runtime->FocusChanged(false);
    }
    std::vector<B21UI_SettingsPanel> SettingsPanels() {
        std::vector<B21UI_SettingsPanel> panels(preview::settings.Panels(nullptr, 0));
        preview::settings.Panels(panels.data(), static_cast<std::uint32_t>(panels.size()));
        return panels;
    }
    const char* SettingsCategory(B21UI_ClientId id) { return preview::settings.Category(id); }
    bool OpenSettings(B21UI_ClientId id) {
        const auto target = preview::settings.Resolve(id);
        if (!target) return false;
        for (std::size_t i = 0; i < preview::Clients().size(); ++i) {
            auto& [client, runtime] = preview::Clients()[i];
            const auto panel = static_cast<B21UI_ClientId>(i + 1);
            if (panel != target && preview::settings.Contains(panel) && !preview::closed.contains(client)) {
                preview::closed.insert(client);
                runtime->FocusChanged(false);
            }
        }
        const auto& [client, runtime] = preview::Clients()[target - 1];
        preview::settings.Select(target);
        if (preview::closed.erase(client)) runtime->FocusChanged(true);
        return true;
    }
    bool IsOpen(const Client& c) { return !preview::closed.contains(&c); }
    void SetPausesGame(Client&, bool) {}
    bool Available() { return true; }
    Device ActiveDevice() { return preview::gamepad ? Device::Gamepad : Device::KeyboardMouse; }
    void SetCursor(float x, float y) {
        // Rounded so the WM_MOUSEMOVE this causes matches and does not switch the device to mouse.
        POINT p{static_cast<LONG>(x), static_cast<LONG>(y)};
        preview::cursor = {static_cast<float>(p.x), static_cast<float>(p.y)};
        ::ClientToScreen(preview::hwnd, &p);
        ::SetCursorPos(p.x, p.y);
    }
    void OnF4SEMessage(const void*) {}

    // Companion windows are logged stand-ins; settings panels run in this process.
    std::vector<const Window*> InstalledWindows(std::string_view self) {
        return InstalledWindows(self, [](const Window& window) {
            return window.id != "ui21Settings" || !SettingsPanels().empty();
        });
    }
    bool OpenWindow(std::string_view id) {
        if (id == "ui21Settings") return OpenSettings();
        std::printf("B21UI preview: open window %.*s\n", static_cast<int>(id.size()), id.data());
        return true;
    }
    bool SwitchToWindow(Client& self, std::string_view id) {
        if (id == "ui21Settings") {
            if (SettingsPanels().empty()) return false;
            Close(self);
        }
        return OpenWindow(id);
    }
}
