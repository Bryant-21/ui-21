#include <F4SE/F4SE.h>
#include <RE/Fallout.h>

#include "b21ui/B21UI.h"
#include "b21ui/Tasks.h"
#include "b21ui/Settings.h"
#include "b21ui/Keybindings.h"
#include "core/Keybindings.h"
#include "client/ClientRuntime.h"
#include "game/GameState.h"
#include "game/Rendezvous.h"
#include "game/Tasks.h"
#include "game/McmRuntime.h"

#include <spdlog/spdlog.h>

#include <Windows.h>

#include <memory>
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace b21ui {
    namespace {
        struct Registered {
            Client* client{};
            std::string name;
            std::string settingsLabel;
            std::string settingsIcon;
            std::string settingsCategory;
            ClientOptions options;
            std::unique_ptr<client::ClientRuntime> runtime;
            B21UI_ClientId id{};
            bool failed{};
        };

        std::mutex mutex;
        std::vector<std::unique_ptr<Registered>> clients;
        const B21UI_HostApi* host{};
        std::uint32_t mainThread{};
        struct KeyProvider {
            std::string id, label, json;
            std::function<std::vector<keys::Binding>()> read;
            bool registered{};
        };
        std::vector<std::unique_ptr<KeyProvider>> keyProviders;

        void RegisterKeysWithHost(KeyProvider& p) {
            if (p.registered || !host || host->size < offsetof(B21UI_HostApi, registerKeybindings) + sizeof(host->registerKeybindings) ||
                !host->registerKeybindings) return;
            const B21UI_KeybindingProvider desc{sizeof(desc), p.id.c_str(), p.label.c_str(), &p, [](void* user) -> const char* {
                auto& provider = *static_cast<KeyProvider*>(user);
                try { provider.json = keys::Encode(provider.read()).dump(); }
                catch (...) { provider.json = "null"; }
                return provider.json.c_str();
            }};
            p.registered = host->registerKeybindings(&desc) != 0;
        }

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
                (r.options.pausesGame ? static_cast<std::uint32_t>(B21UI_FLAG_PAUSES_GAME) : 0u) |
                (r.options.settings ? static_cast<std::uint32_t>(B21UI_FLAG_SETTINGS) : 0u),
                &r, &RenderThunk, &DeviceLostThunk, &FocusThunk, r.settingsLabel.c_str(), r.settingsIcon.c_str(),
                r.settingsCategory.c_str()};
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
        if (options.settings && !options.modal) return false;
        std::scoped_lock guard(mutex);
        if (Find(client)) return true;
        auto r = std::make_unique<Registered>();
        r->client = &client;
        r->name = options.name ? options.name : "";
        r->settingsLabel = options.settingsLabel ? options.settingsLabel : "";
        r->settingsIcon = options.settingsIcon ? options.settingsIcon : "";
        r->settingsCategory = options.settingsCategory ? options.settingsCategory : "UI 21";
        r->options = options;
        r->options.name = nullptr;
        r->runtime = std::make_unique<client::ClientRuntime>(client, r->name, options.padPointer,
                                                                  options.scaleWithResolution, options.settings);
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
    bool keys::Register(const char* provider, const char* label, std::function<std::vector<Binding>()> read) {
        if (!provider || !*provider || !read) return false;
        std::scoped_lock guard(mutex);
        if (std::ranges::any_of(keyProviders, [&](const auto& p) { return p->id == provider; })) return false;
        auto p = std::make_unique<KeyProvider>();
        p->id = provider; p->label = label ? label : provider; p->read = std::move(read);
        RegisterKeysWithHost(*p);
        keyProviders.push_back(std::move(p)); return true;
    }
    std::vector<B21UI_SettingsPanel> SettingsPanels() {
        if (!host || host->size < offsetof(B21UI_HostApi, settingsPanels) + sizeof(host->settingsPanels) ||
            !host->settingsPanels) return {};
        std::vector<B21UI_SettingsPanel> panels(host->settingsPanels(nullptr, 0));
        const auto count = host->settingsPanels(panels.data(), static_cast<std::uint32_t>(panels.size()));
        panels.resize(std::min<std::size_t>(count, panels.size()));
        return panels;
    }
    bool OpenSettings(B21UI_ClientId id) {
        if (!host || host->size < offsetof(B21UI_HostApi, openSettings) + sizeof(host->openSettings) ||
            !host->openSettings || SettingsPanels().empty() || game::GameState::ModalBlockedReason()) return false;
        if (::GetCurrentThreadId() == mainThread) return host->openSettings(id) != 0;
        OnMainThread([id] { if (host) host->openSettings(id); });
        return true;
    }
    const char* SettingsCategory(B21UI_ClientId id) {
        return host && host->size >= offsetof(B21UI_HostApi, settingsCategory) + sizeof(host->settingsCategory) &&
            host->settingsCategory ? host->settingsCategory(id) : "UI 21";
    }
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
            for (auto& p : keyProviders) RegisterKeysWithHost(*p);
            if (game::IsHost()) game::McmRuntime::RegisterPapyrus();
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
