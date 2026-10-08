#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>
#include "client/InputMap.h"

#include <vector>

using namespace b21ui::client;

namespace {
    struct ContextGuard {
        ContextGuard() { ImGui::CreateContext(); }
        ~ContextGuard() { ImGui::DestroyContext(); }
    };
    B21UI_Frame FrameWith(const std::vector<B21UI_Event>& events) {
        B21UI_Frame f{};
        f.size = sizeof(B21UI_Frame);
        f.displayWidth = 1920;
        f.displayHeight = 1080;
        f.cursorX = 100;
        f.cursorY = 200;
        f.events = events.data();
        f.eventCount = static_cast<std::uint32_t>(events.size());
        return f;
    }
}

TEST_CASE("virtual keys map to ImGui keys") {
    CHECK(KeyFromVk(0x41) == ImGuiKey_A);
    CHECK(KeyFromVk(0x39) == ImGuiKey_9);
    CHECK(KeyFromVk(0x70) == ImGuiKey_F1);
    CHECK(KeyFromVk(0x0D) == ImGuiKey_Enter);
    CHECK(KeyFromVk(0x1B) == ImGuiKey_Escape);
    CHECK(KeyFromVk(0x25) == ImGuiKey_LeftArrow);
    CHECK(KeyFromVk(0xA2) == ImGuiKey_LeftCtrl);
    CHECK(KeyFromVk(0xFF) == ImGuiKey_None);
}

TEST_CASE("pad buttons map to ImGui gamepad keys") {
    CHECK(KeyFromPad(B21UI_PAD_A) == ImGuiKey_GamepadFaceDown);
    CHECK(KeyFromPad(B21UI_PAD_B) == ImGuiKey_GamepadFaceRight);
    CHECK(KeyFromPad(B21UI_PAD_LB) == ImGuiKey_GamepadL1);
    CHECK(KeyFromPad(B21UI_PAD_RT) == ImGuiKey_GamepadR2);
    CHECK(KeyFromPad(B21UI_PAD_DPAD_LEFT) == ImGuiKey_GamepadDpadLeft);
}

TEST_CASE("ApplyEvents queues mouse, key, char and pad input") {
    ContextGuard guard;
    auto& io = ImGui::GetIO();
    const std::vector<B21UI_Event> events{
        {B21UI_EV_MOUSE_BUTTON, 0, 1, 0, 0},
        {B21UI_EV_MOUSE_WHEEL, 0, 1, 0, -1.0F},
        {B21UI_EV_KEY, 0x41, 1, 0, 0},
        {B21UI_EV_CHAR, 0xE9, 1, 0, 0},
        {B21UI_EV_PAD_BUTTON, B21UI_PAD_A, 1, 1.0F, 0},
        {B21UI_EV_PAD_STICK, B21UI_STICK_LEFT, 1, 0.9F, 0.0F},
    };
    StickState sticks{};
    ApplyEvents(io, FrameWith(events), sticks);
    const auto& queue = ImGui::GetCurrentContext()->InputEventsQueue;
    int mousePos = 0, mouseButton = 0, wheel = 0, key = 0, text = 0;
    for (const auto& e : queue) {
        mousePos += e.Type == ImGuiInputEventType_MousePos;
        mouseButton += e.Type == ImGuiInputEventType_MouseButton;
        wheel += e.Type == ImGuiInputEventType_MouseWheel;
        key += e.Type == ImGuiInputEventType_Key;
        text += e.Type == ImGuiInputEventType_Text;
    }
    CHECK(mousePos == 1);
    CHECK(mouseButton == 1);
    CHECK(wheel == 1);
    CHECK(key >= 2);   // 'A' + FaceDown + stick analog keys
    CHECK(text == 1);
    CHECK(sticks.lx == doctest::Approx(0.9F));
}

TEST_CASE("focus loss is forwarded") {
    ContextGuard guard;
    StickState sticks{};
    const std::vector<B21UI_Event> events{{B21UI_EV_FOCUS, 0, 0, 0, 0}};
    ApplyEvents(ImGui::GetIO(), FrameWith(events), sticks);
    bool sawFocus = false;
    for (const auto& e : ImGui::GetCurrentContext()->InputEventsQueue)
        sawFocus |= e.Type == ImGuiInputEventType_Focus;
    CHECK(sawFocus);
}
