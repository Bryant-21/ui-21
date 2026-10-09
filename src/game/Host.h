#pragma once
#include "b21ui/Abi.h"
#include "core/Focus.h"
#include "core/InputTranslate.h"
#include "core/SettingsRegistry.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace b21ui::game {
    class Host {
    public:
        static Host& Get();

        void Start();
        B21UI_ClientId RegisterClient(const B21UI_ClientDesc& desc);
        bool Open(B21UI_ClientId id);
        bool OpenSettings(B21UI_ClientId id);
        std::uint32_t SettingsPanels(B21UI_SettingsPanel* panels, std::uint32_t capacity);
        const char* SettingsCategory(B21UI_ClientId id);
        void Close(B21UI_ClientId id);
        void SetPausesGame(B21UI_ClientId id, bool pausesGame);
        bool IsOpen(B21UI_ClientId id);
        [[nodiscard]] bool Available() const { return available_; }
        [[nodiscard]] std::uint32_t ActiveDevice();
        void SetCursor(float x, float y);
        [[nodiscard]] bool WantsGameState();

        void OnRawInput(const core::RawInput& input);
        void OnWindowFocus(bool focused);
        void CloseFocusedModal(const char* reason);
        // modalPass: true = draw the focused modal client, false = draw open overlays.
        void RenderClients(bool modalPass, ID3D11Device* device, ID3D11DeviceContext* context, float width, float height,
                           float deltaSeconds, std::uint32_t frameFlags);
        void DeviceLost();
        void SetRenderAvailable(bool available) { available_ = available; }

    private:
        struct Client {
            B21UI_ClientDesc desc{};
            std::string name;
        };
        void ApplyGameState();
        Client* Find(B21UI_ClientId id);

        std::mutex mutex_;
        core::FocusArbiter focus_;
        core::SettingsRegistry settings_;
        core::InputTranslator translator_;
        std::vector<B21UI_Event> pending_;
        std::vector<Client> clients_;
        bool available_{};
        std::atomic<bool> wantsGameState_{};
        float lastWidth_{};
        float lastHeight_{};
    };

    // Maps game cursor units to render-target pixels (M0 Q4) and back.
    void CursorToPixels(float rawX, float rawY, float width, float height, float& x, float& y);
    void PixelsToCursor(float x, float y, float width, float height, float& rawX, float& rawY);
}
