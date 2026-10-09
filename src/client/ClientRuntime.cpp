#include "client/ClientRuntime.h"
#include "client/PadKeyboardView.h"
#include "b21ui/fo76/Style.h"
#include "b21ui/Paths.h"
#include "b21ui/modern/Theme.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstddef>

namespace b21ui::client {
    ClientRuntime::ClientRuntime(Client& client, std::string name, bool padPointer, bool scaleWithResolution, bool settings)
        : client_(client), name_(std::move(name)), scaleWithResolution_(scaleWithResolution), settings_(settings), pointer_(padPointer) {}

    ClientRuntime::~ClientRuntime() {
        if (!context_) return;
        auto* previous = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(context_);
        if (backend_) ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext(context_);
        ImGui::SetCurrentContext(previous == context_ ? nullptr : previous);
    }

    FrameContext ClientRuntime::Context(const B21UI_Frame& f) const {
        return {f.displayWidth, f.displayHeight, f.deltaSeconds, f.cursorX, f.cursorY,
                static_cast<Device>(f.activeDevice), f.focused != 0, f.device, f.context,
                sticks_.lx, sticks_.ly, sticks_.rx, sticks_.ry};
    }

    void ClientRuntime::Render(const B21UI_Frame& frame) {
        if (!frame.device || !frame.context || frame.displayWidth <= 0 || frame.displayHeight <= 0) return;
        auto* previous = ImGui::GetCurrentContext();
        if (!context_) {
            context_ = ImGui::CreateContext();
            ImGui::SetCurrentContext(context_);
            fo76::InitContext(PluginAssetDir() / "fonts");
        }
        ImGui::SetCurrentContext(context_);
        if (device_ != frame.device) {
            if (backend_) ImGui_ImplDX11_Shutdown();
            backend_ = ImGui_ImplDX11_Init(frame.device, frame.context);
            device_ = frame.device;
        }
        if (!backend_) { ImGui::SetCurrentContext(previous); return; }
        auto& io = ImGui::GetIO();
        io.DisplaySize = {frame.displayWidth, frame.displayHeight};
        common::SetUiScale(scaleWithResolution_ ? std::max(1.0F, frame.displayHeight / 1080.0F) : 1.0F);
        common::SetActiveDevice(frame.activeDevice);
        if (frame.size >= offsetof(B21UI_Frame, hudColor) + sizeof(frame.hudColor) && (frame.hudColor >> 24) != 0) {
            const auto c = frame.hudColor;
            common::SetHudColor(IM_COL32(c >> 16 & 0xFF, c >> 8 & 0xFF, c & 0xFF, 255));
        }
        ImGui::GetStyle().MouseCursorScale = std::max(1.0F, frame.displayHeight / 720.0F);
        io.DeltaTime = frame.deltaSeconds > 0.0F ? frame.deltaSeconds : 1.0F / 60.0F;
        // The host drops input queued for a closing client, so key-ups never arrive; without this a key
        // held at close stays down and ImGui's key repeat fires it again on reopen.
        if (resetInput_.exchange(false)) {
            io.ClearEventsQueue();
            io.ClearInputKeys();
            io.ClearInputMouse();
            sticks_ = {};
            pointer_.Reset();
            keyboard_ = {};
            settingsHost_.Reset();
        }
        const auto& pad = common::PadTuning();
        // A text field the controller is typing into gets the on-screen keyboard, which takes the pad
        // input (io.WantTextInput is last frame's; the field is still active until Enter lands).
        const bool typing = io.WantTextInput && frame.activeDevice == B21UI_DEVICE_GAMEPAD;
        keyboardEvents_.assign(frame.events, frame.events + frame.eventCount);
        keyboard_.Process(typing, keyboardEvents_);
        auto padFrame = frame;
        padFrame.events = keyboardEvents_.data();
        padFrame.eventCount = static_cast<std::uint32_t>(keyboardEvents_.size());
        const auto pointer = pointer_.Process(padFrame, {pad.deadzone, pad.cursorSpeed, pad.panSpeed, pad.exponent, common::PointerMultiplier()}, events_);
        if (pointer.moved) b21ui::SetCursor(pointer.x, pointer.y);
        auto input = frame;
        input.events = events_.data();
        input.eventCount = static_cast<std::uint32_t>(events_.size());
        input.cursorX = pointer.x;
        input.cursorY = pointer.y;
        // M0: the game cursor is not a usable pointer under our UI, so B21UI always draws its own.
        // Free-cursor views (map canvas) set MouseDrawCursor = true for gamepad inside their frame.
        io.MouseDrawCursor = (frame.flags & B21UI_FRAME_DRAW_CURSOR) != 0 &&
                             (frame.activeDevice == B21UI_DEVICE_KEYBOARD_MOUSE || pointer.active);
        ApplyEvents(io, input, sticks_);
        const auto ctx = Context(input);
        if (!created_) {
            created_ = true;
            client_.OnContextCreated(ctx);
            if (settings_) modern::theme::LoadFonts(PluginAssetDir() / "fonts");
        }
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        if (settings_) settingsHost_.Draw(client_, ctx);
        else client_.Draw(ctx);
        if (GImGui->PlatformImeData.WantTextInput && frame.activeDevice == B21UI_DEVICE_GAMEPAD)
            DrawPadKeyboard(keyboard_, common::UiScale());
        // The game-style pointer replaces ImGui's arrow; other shapes (text beam, resize) stay ImGui's.
        if (io.MouseDrawCursor && ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow && ImGui::IsMousePosValid()) {
            io.MouseDrawCursor = false;
            common::DrawGameCursor(ImGui::GetForegroundDrawList(), io.MousePos, common::HudColor(), ImGui::GetStyle().MouseCursorScale);
        }
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        ImGui::SetCurrentContext(previous);
    }

    void ClientRuntime::DeviceLost() {
        if (!context_) return;
        auto* previous = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(context_);
        if (backend_) ImGui_ImplDX11_Shutdown();
        common::ReleaseDeviceObjects();
        backend_ = false;
        device_ = nullptr;
        created_ = false;
        client_.OnDeviceLost();
        ImGui::SetCurrentContext(previous);
    }

    void ClientRuntime::FocusChanged(bool focused) {
        if (!focused) resetInput_ = true;
        else common::RefreshPointerMultiplier();
        client_.OnFocusChanged(focused);
    }
}
