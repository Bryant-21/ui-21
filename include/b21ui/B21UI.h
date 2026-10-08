#pragma once
#include "b21ui/Abi.h"

#include <cstdint>

namespace F4SE { class MessagingInterface; }

namespace b21ui {
    enum class Device : std::uint8_t { KeyboardMouse = B21UI_DEVICE_KEYBOARD_MOUSE, Gamepad = B21UI_DEVICE_GAMEPAD };

    struct FrameContext {
        float width{};
        float height{};
        float deltaSeconds{};
        float cursorX{};
        float cursorY{};
        Device device{};
        bool focused{};
        ID3D11Device* d3d{};
        ID3D11DeviceContext* context{};
        float leftStickX{}, leftStickY{}, rightStickX{}, rightStickY{};   // raw, +y = up
    };

    struct ClientOptions {
        const char* name = "";
        bool modal = true;
        bool pausesGame = false;
        // Right stick shows and moves a pointer that A clicks. Views that drive their own pointer
        // (the map) turn this off.
        bool padPointer = true;
        // Kit sizes grow with the display (height / 1080, never below 1). Views laid out in physical
        // pixels, like the map's former web UI, turn this off and scale with the text scale alone.
        bool scaleWithResolution = true;
    };

    class Client {
    public:
        virtual ~Client() = default;
        // Called every frame the client is open, with this client's ImGui context current.
        virtual void Draw(const FrameContext& frame) = 0;
        // Called once after the context, fonts and style exist; load textures here.
        virtual void OnContextCreated(const FrameContext&) {}
        virtual void OnFocusChanged(bool) {}
        // D3D objects the client created are invalid; drop them (they are reloaded lazily).
        virtual void OnDeviceLost() {}
    };

    // Must be called before the game reaches kPostLoad completion or any time after; calls made
    // before the rendezvous are queued. `client` must outlive the plugin.
    bool Register(Client& client, const ClientOptions& options);
    // Main-thread safe from any thread (marshaled). Returns false if another modal holds focus
    // or no host is available.
    bool Open(Client& client);
    void Close(Client& client);
    void SetPausesGame(Client& client, bool pausesGame);
    bool IsOpen(const Client& client);
    bool Available();
    Device ActiveDevice();
    // Moves the shared game cursor (render-target pixels).
    void SetCursor(float x, float y);
    // Forward every F4SE message from the plugin's listener.
    void OnF4SEMessage(const void* f4seMessage);

    // F4SE plugin message (no data) asking the receiving mod to open its main B21UI window, as its
    // hotkey would. Sent on the game thread, e.g. by the Dev Tools launcher.
    inline constexpr std::uint32_t kOpenWindowMessage = 0x4232574FU;  // 'B2WO'
}
