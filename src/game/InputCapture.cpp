#include <F4SE/F4SE.h>
#include <RE/Fallout.h>

#include "game/InputCapture.h"
#include "game/Host.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <set>

namespace b21ui::game::InputCapture {
    namespace {
        core::RawDevice Device(RE::INPUT_DEVICE d) {
            switch (d) {
            case RE::INPUT_DEVICE::kKeyboard: return core::RawDevice::Keyboard;
            case RE::INPUT_DEVICE::kMouse: return core::RawDevice::Mouse;
            case RE::INPUT_DEVICE::kGamepad: return core::RawDevice::Gamepad;
            default: return core::RawDevice::Other;
            }
        }

        class Sink : public RE::BSInputEventUser {
        public:
            bool ShouldHandleEvent(const RE::InputEvent* e) override {
                if (!e) return false;
                if (Host::Get().WantsGameState()) return true;
                const auto* button = e->eventType == RE::INPUT_EVENT_TYPE::kButton ? e->As<RE::ButtonEvent>() : nullptr;
                std::scoped_lock guard(heldLock_);
                return button && held_.contains(Key(*button));
            }
            void OnButtonEvent(const RE::ButtonEvent* e) override {
                const_cast<RE::ButtonEvent*>(e)->disabled = true;
                {
                    // A key that closes the UI (Esc, B) is still down afterwards; its repeats and its
                    // release must not reach the game, or Esc opens the pause menu.
                    std::scoped_lock guard(heldLock_);
                    if (e->value > 0.0F) held_.insert(Key(*e));
                    else held_.erase(Key(*e));
                }
                if (!Host::Get().WantsGameState()) return;
                Host::Get().OnRawInput({core::RawKind::Button, Device(e->device.get()), static_cast<std::uint32_t>(e->idCode), e->value, e->heldDownSecs, 0, 0, 0});
            }
            void OnThumbstickEvent(const RE::ThumbstickEvent* e) override {
                const_cast<RE::ThumbstickEvent*>(e)->disabled = true;
                Host::Get().OnRawInput({core::RawKind::Thumbstick, core::RawDevice::Gamepad, static_cast<std::uint32_t>(e->idCode), 0, 0, e->xValue, e->yValue, 0});
            }
            void OnCharacterEvent(const RE::CharacterEvent* e) override {
                Host::Get().OnRawInput({core::RawKind::Character, core::RawDevice::Keyboard, 0, 0, 0, 0, 0, e->charCode});
            }
            void OnMouseMoveEvent(const RE::MouseMoveEvent* e) override {
                Host::Get().OnRawInput({core::RawKind::MouseMove, core::RawDevice::Mouse, 0, 0, 0,
                                        static_cast<float>(e->mouseInputX), static_cast<float>(e->mouseInputY), 0});
            }

        private:
            static std::uint64_t Key(const RE::ButtonEvent& e) {
                return (static_cast<std::uint64_t>(e.device.underlying()) << 32) | static_cast<std::uint32_t>(e.idCode);
            }

            std::mutex heldLock_;
            std::set<std::uint64_t> held_;
        };
        Sink sink;
    }

    void Install() { EnsureFront(); }

    void EnsureFront() {
        auto* controls = RE::MenuControls::GetSingleton();
        if (!controls) return;
        auto& handlers = controls->handlers;
        auto* self = static_cast<RE::BSInputEventUser*>(&sink);
        if (!handlers.empty() && handlers.front() == self) return;
        if (const auto it = std::find(handlers.begin(), handlers.end(), self); it != handlers.end()) handlers.erase(it);
        handlers.insert(handlers.begin(), self);
    }
}
