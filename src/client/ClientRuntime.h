#pragma once
#include "b21ui/B21UI.h"
#include "client/InputMap.h"
#include "client/PadPointer.h"
#include "core/PadKeyboard.h"

#include <atomic>
#include <string>
#include <vector>

struct ImGuiContext;

namespace b21ui::client {
    class ClientRuntime {
    public:
        ClientRuntime(Client& client, std::string name, bool padPointer = true, bool scaleWithResolution = true);
        ~ClientRuntime();
        ClientRuntime(const ClientRuntime&) = delete;
        ClientRuntime& operator=(const ClientRuntime&) = delete;

        void Render(const B21UI_Frame& frame);
        void DeviceLost();
        void FocusChanged(bool focused);
        [[nodiscard]] const std::string& Name() const { return name_; }

    private:
        FrameContext Context(const B21UI_Frame& frame) const;

        Client& client_;
        std::string name_;
        bool scaleWithResolution_{true};
        ImGuiContext* context_{};
        ID3D11Device* device_{};
        bool backend_{};
        bool created_{};
        StickState sticks_{};
        std::atomic<bool> resetInput_{};
        PadPointer pointer_;
        core::PadKeyboard keyboard_;
        std::vector<B21UI_Event> keyboardEvents_;
        std::vector<B21UI_Event> events_;
    };
}
