#pragma once

namespace b21ui::game::GameState {
    void RegisterMenu();          // once, at kGameDataReady
    void Enter(bool pauseGame);   // show stub menu, lock controls, text-entry mode, optional freeze
    void Leave();
    void SetPausesGame(bool pausesGame);
    bool Active();
    // Why a modal must not open right now (loading screen, main menu, no save loaded); nullptr when it may.
    const char* ModalBlockedReason();
}
