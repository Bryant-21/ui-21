#include "core/Focus.h"

namespace b21ui::core {
    std::uint32_t FocusArbiter::Add(ClientKind kind, bool pausesGame) {
        const auto id = static_cast<std::uint32_t>(entries_.size() + 1);
        entries_.push_back({id, kind, pausesGame, false});
        return id;
    }

    FocusArbiter::Entry* FocusArbiter::Find(std::uint32_t id) {
        return id >= 1 && id <= entries_.size() ? &entries_[id - 1] : nullptr;
    }

    const FocusArbiter::Entry* FocusArbiter::Find(std::uint32_t id) const {
        return id >= 1 && id <= entries_.size() ? &entries_[id - 1] : nullptr;
    }

    bool FocusArbiter::Open(std::uint32_t id) {
        auto* e = Find(id);
        if (!e) return false;
        if (e->kind == ClientKind::Overlay) {
            e->open = true;
            return true;
        }
        if (focused_ != 0 && focused_ != id) return false;
        e->open = true;
        focused_ = id;
        return true;
    }

    void FocusArbiter::Close(std::uint32_t id) {
        if (auto* e = Find(id)) e->open = false;
        if (focused_ == id) focused_ = 0;
    }

    bool FocusArbiter::SetPausesGame(std::uint32_t id, bool pausesGame) {
        auto* entry = Find(id);
        if (!entry) return false;
        entry->pausesGame = pausesGame;
        return true;
    }

    void FocusArbiter::CloseAll() {
        for (auto& e : entries_) e.open = false;
        focused_ = 0;
    }

    bool FocusArbiter::IsOpen(std::uint32_t id) const {
        const auto* e = Find(id);
        return e && e->open;
    }

    bool FocusArbiter::WantsPause() const {
        const auto* e = Find(focused_);
        return e && e->pausesGame;
    }

    std::vector<std::uint32_t> FocusArbiter::RenderOrder() const {
        std::vector<std::uint32_t> order;
        for (const auto& e : entries_)
            if (e.open && e.kind == ClientKind::Overlay) order.push_back(e.id);
        if (focused_ != 0) order.push_back(focused_);
        return order;
    }
}
