#pragma once
#include "core/Mcm.h"
#include <array>
#include <functional>

namespace b21ui::mcm {
    class View {
    public:
        using Submit = std::function<void(std::size_t, const Json&)>;
        static bool Navigation(const Menu& menu, std::size_t& page);
        void Draw(const Menu& menu, std::size_t page, const Submit& submit);
    private:
        int capture_ = -1;
        bool captureArmed_{};
        std::map<int, std::array<char, 4096>> buffers_;
        std::map<int, Json> edits_;
        std::string page_;
    };
}
