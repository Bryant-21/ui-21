#pragma once
#include <cstdint>
#include <vector>

namespace b21ui::core {
    enum class ClientKind : std::uint8_t { Modal, Overlay };

    class FocusArbiter {
    public:
        std::uint32_t Add(ClientKind kind, bool pausesGame);
        bool Open(std::uint32_t id);
        void Close(std::uint32_t id);
        bool SetPausesGame(std::uint32_t id, bool pausesGame);
        void CloseAll();
        [[nodiscard]] bool IsOpen(std::uint32_t id) const;
        [[nodiscard]] std::uint32_t Focused() const { return focused_; }
        [[nodiscard]] bool WantsGameState() const { return focused_ != 0; }
        [[nodiscard]] bool WantsPause() const;
        [[nodiscard]] std::vector<std::uint32_t> RenderOrder() const;

    private:
        struct Entry {
            std::uint32_t id{};
            ClientKind kind{};
            bool pausesGame{};
            bool open{};
        };
        Entry* Find(std::uint32_t id);
        const Entry* Find(std::uint32_t id) const;

        std::vector<Entry> entries_;
        std::uint32_t focused_{};
    };
}
