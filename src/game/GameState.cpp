#include <F4SE/F4SE.h>
#include <RE/Fallout.h>

#include "game/GameState.h"

#include <atomic>

namespace b21ui::game::GameState {
    namespace {
        constexpr auto kMenuName = "B21UI_CursorMenu";
        // Main::freezeTime alone left the world running in game (M1 gate); a kPausesGame menu is
        // the engine's own pause, the same one the Pip-Boy uses.
        constexpr auto kPauseMenuName = "B21UI_PauseCursorMenu";
        RE::BSTSmartPointer<RE::BSInputEnableLayer> lock;
        std::atomic<bool> active{};
        const char* shownMenu{};

        void SetGameCursorVisible(bool visible);

        template <bool Pauses>
        class StubMenu : public RE::IMenu {
        public:
            StubMenu() {
                menuFlags.set(RE::UI_MENU_FLAGS::kUsesCursor, RE::UI_MENU_FLAGS::kUpdateUsesCursor,
                              RE::UI_MENU_FLAGS::kCustomRendering);
                if constexpr (Pauses) menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame);
                menuName = Pauses ? kPauseMenuName : kMenuName;
                inputEventHandlingEnabled = false;
            }
            static RE::IMenu* Create(const RE::UIMessage&) { return new StubMenu(); }

            // The CursorMenu re-shows its arrow when menus open and refresh, so the hide is re-applied
            // every UI frame while B21UI holds the game state.
            void AdvanceMovie(float a_timeDelta, std::uint64_t a_time) override {
                RE::IMenu::AdvanceMovie(a_timeDelta, a_time);
                if (active) SetGameCursorVisible(false);
            }
        };

        void Message(const char* menu, RE::UI_MESSAGE_TYPE type) {
            if (auto* queue = RE::UIMessageQueue::GetSingleton()) queue->AddMessage(RE::BSFixedString(menu), type);
        }

        // B21UI draws its own cursor, so the game's arrow is hidden while it is up. The CursorMenu
        // keeps running underneath, which keeps MenuCursor's position current.
        void SetGameCursorVisible(bool visible) {
            auto* ui = RE::UI::GetSingleton();
            if (!ui) return;
            const auto menu = ui->GetMenu(RE::BSFixedString("CursorMenu"));
            if (!menu || !menu->uiMovie) return;
            // FO4's AS3 movies expose their root as root1; older ones as root.
            for (const auto* path : {"root1", "root"}) {
                Scaleform::GFx::Value root;
                if (menu->uiMovie->GetVariable(&root, path) && (root.IsDisplayObject() || root.IsObject())) {
                    root.SetMember("visible", Scaleform::GFx::Value(visible));
                    return;
                }
            }
        }
    }

    void RegisterMenu() {
        if (auto* ui = RE::UI::GetSingleton()) {
            ui->RegisterMenu(kMenuName, StubMenu<false>::Create);
            ui->RegisterMenu(kPauseMenuName, StubMenu<true>::Create);
        }
    }

    void Enter(bool pauseGame) {
        if (active) return;
        active = true;
        shownMenu = pauseGame ? kPauseMenuName : kMenuName;
        Message(shownMenu, RE::UI_MESSAGE_TYPE::kShow);
        if (auto* manager = RE::BSInputEnableManager::GetSingleton(); manager && !lock) {
            manager->AllocateNewLayer(lock, "B21UI");
            if (lock)
                manager->EnableUserEvent(lock->layerID, RE::UserEvents::USER_EVENT_FLAG::kAll, false,
                                         RE::UserEvents::SENDER_ID::kMenu);
        }
        if (auto* controlMap = RE::ControlMap::GetSingleton()) controlMap->SetTextEntryMode(true);
        if (auto* ui = RE::UI::GetSingleton()) ui->RefreshCursor();
        SetGameCursorVisible(false);
    }

    void Leave() {
        if (!active) return;
        active = false;
        Message(shownMenu, RE::UI_MESSAGE_TYPE::kHide);
        shownMenu = nullptr;
        lock.reset();
        if (auto* controlMap = RE::ControlMap::GetSingleton()) controlMap->SetTextEntryMode(false);
        SetGameCursorVisible(true);
    }

    void SetPausesGame(bool pausesGame) {
        if (!active) return;
        const auto* nextMenu = pausesGame ? kPauseMenuName : kMenuName;
        if (shownMenu == nextMenu) return;
        Message(nextMenu, RE::UI_MESSAGE_TYPE::kShow);
        Message(shownMenu, RE::UI_MESSAGE_TYPE::kHide);
        shownMenu = nextMenu;
    }

    bool Active() { return active; }

    // Modals read and drive the loaded game: at the main menu the player exists but is not set up,
    // and DevTools crashed reading its actor values there.
    const char* ModalBlockedReason() {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) return "the UI is not ready";
        if (ui->GetMenuOpen(RE::LoadingMenu::MENU_NAME)) return "a loading screen";
        if (ui->GetMenuOpen(RE::MainMenu::MENU_NAME)) return "the main menu";
        const auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->parentCell) return "no game is loaded";
        return nullptr;
    }
}
