#include <F4SE/F4SE.h>
#include <RE/Fallout.h>

#include "b21ui/B21UI.h"
#include "b21ui/Tasks.h"
#include "client/ClientRuntime.h"
#include "game/GameState.h"
#include "game/Rendezvous.h"
#include "game/Tasks.h"

#include <spdlog/spdlog.h>

#include <Windows.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace b21ui {
    namespace {
        struct Registered {
            Client* client{};
            std::string name;
            ClientOptions options;
            std::unique_ptr<client::ClientRuntime> runtime;
            B21UI_ClientId id{};
            bool failed{};
        };

        std::mutex mutex;
        std::vector<std::unique_ptr<Registered>> clients;
        const B21UI_HostApi* host{};
        std::uint32_t mainThread{};

        Registered* Find(const Client& c) {
            for (auto& r : clients) if (r->client == &c) return r.get();
            return nullptr;
        }

        void RenderThunk(void* user, const B21UI_Frame* frame) noexcept {
            auto* r = static_cast<Registered*>(user);
            if (r->failed || !frame) return;
            try {
                r->runtime->Render(*frame);
            } catch (const std::exception& e) {
                r->failed = true;
                spdlog::error("B21UI client '{}' disabled after exception: {}", r->name, e.what());
            } catch (...) {
                r->failed = true;
                spdlog::error("B21UI client '{}' disabled after unknown exception", r->name);
            }
        }
        void DeviceLostThunk(void* user) noexcept {
            try { static_cast<Registered*>(user)->runtime->DeviceLost(); } catch (...) {}
        }
        void FocusThunk(void* user, std::uint32_t focused) noexcept {
            try { static_cast<Registered*>(user)->runtime->FocusChanged(focused != 0); } catch (...) {}
        }

        void RegisterWithHost(Registered& r) {
            if (!host || r.id) return;
            const B21UI_ClientDesc desc{sizeof(B21UI_ClientDesc), r.name.c_str(),
                r.options.modal ? static_cast<std::uint32_t>(B21UI_KIND_MODAL) : static_cast<std::uint32_t>(B21UI_KIND_OVERLAY),
                r.options.pausesGame ? static_cast<std::uint32_t>(B21UI_FLAG_PAUSES_GAME) : 0u,
                &r, &RenderThunk, &DeviceLostThunk, &FocusThunk};
            r.id = host->registerClient(&desc);
            if (!r.id) spdlog::warn("B21UI: host refused client '{}'", r.name);
        }

        template <class Fn>
        void OnMainThread(Fn&& fn) {
            if (::GetCurrentThreadId() == mainThread || !mainThread) { fn(); return; }
            QueueGameTask(std::forward<Fn>(fn));
        }
    }

    bool Register(Client& client, const ClientOptions& options) {
        std::scoped_lock guard(mutex);
        if (Find(client)) return true;
        auto r = std::make_unique<Registered>();
        r->client = &client;
        r->name = options.name ? options.name : "";
        r->options = options;
        r->options.name = nullptr;
        r->runtime = std::make_unique<client::ClientRuntime>(client, r->name, options.padPointer,
                                                                  options.scaleWithResolution);
        RegisterWithHost(*r);
        clients.push_back(std::move(r));
        return true;
    }

    namespace {
        // Host calls happen outside `mutex`: they reach engine code that takes the input locks the
        // game's input threads hold while calling our hotkey handlers (which call back in here).
        B21UI_ClientId IdOf(const Client& client) {
            std::scoped_lock guard(mutex);
            const auto* r = Find(client);
            return r ? r->id : 0;
        }

        bool IsModal(const Client& client) {
            std::scoped_lock guard(mutex);
            const auto* r = Find(client);
            return r && r->options.modal;
        }
    }

    bool Open(Client& client) {
        const auto id = IdOf(client);
        if (!host || !id) return false;
        // Checked at request time too: an open deferred from an input thread would otherwise run
        // after the load finishes, when the host no longer sees the loading screen.
        if (IsModal(client) && game::GameState::ModalBlockedReason()) return false;
        if (::GetCurrentThreadId() == mainThread) return host->open(id) != 0;
        OnMainThread([id] { if (host) host->open(id); });
        return true;
    }

    // Always deferred: clients call Close from inside their own Draw, which runs in the host's
    // render pass; closing synchronously there would mutate the client list mid-iteration.
    void Close(Client& client) {
        const auto id = IdOf(client);
        if (!host || !id) return;
        QueueGameTask([id] { if (host) host->close(id); });
    }

    void SetPausesGame(Client& client, bool pausesGame) {
        B21UI_ClientId id{};
        {
            std::scoped_lock guard(mutex);
            auto* registered = Find(client);
            if (!registered) return;
            registered->options.pausesGame = pausesGame;
            id = registered->id;
        }
        if (!host || !id || host->size < offsetof(B21UI_HostApi, setPausesGame) + sizeof(host->setPausesGame) ||
            !host->setPausesGame) return;
        OnMainThread([id, pausesGame] { if (host) host->setPausesGame(id, pausesGame ? 1u : 0u); });
    }

    bool IsOpen(const Client& client) {
        const auto id = IdOf(client);
        return host && id && host->isOpen(id) != 0;
    }

    bool Available() { return host && host->available() != 0; }
    Device ActiveDevice() { return host ? static_cast<Device>(host->activeDevice()) : Device::KeyboardMouse; }
    void SetCursor(float x, float y) { if (host) host->setCursor(x, y); }

    void OnF4SEMessage(const void* raw) {
        const auto* message = static_cast<const F4SE::MessagingInterface::Message*>(raw);
        if (!message) return;
        switch (message->type) {
        case F4SE::MessagingInterface::kPostLoad: {
            std::scoped_lock guard(mutex);
            host = game::Rendezvous();
            game::SetGameTaskHost(host);
            for (auto& r : clients) RegisterWithHost(*r);
            break;
        }
        case F4SE::MessagingInterface::kGameDataReady:
            mainThread = ::GetCurrentThreadId();
            if (game::IsHost()) game::StartHost();
            break;
        default:
            break;
        }
    }
}
