#include "core/ListModel.h"

#include <algorithm>

namespace b21ui::core {
    void ListModel::SetCount(int count) {
        count_ = std::max(0, count);
        selected_ = count_ == 0 ? -1 : std::clamp(selected_ < 0 ? 0 : selected_, 0, count_ - 1);
        Follow();
    }

    void ListModel::SetVisibleRows(int rows) {
        visible_ = std::max(1, rows);
        Follow();
    }

    void ListModel::Select(int index) {
        if (count_ == 0) return;
        selected_ = std::clamp(index, 0, count_ - 1);
        Follow();
    }

    void ListModel::Move(int delta, bool wrap) {
        if (count_ == 0 || delta == 0) return;
        const int next = selected_ + delta;
        selected_ = wrap ? ((next % count_) + count_) % count_ : std::clamp(next, 0, count_ - 1);
        Follow();
    }

    void ListModel::Page(int direction) { Move(direction * visible_, false); }

    int ListModel::Last() const { return std::min(count_, first_ + visible_); }

    void ListModel::Follow() {
        if (selected_ < first_) first_ = selected_;
        if (selected_ >= first_ + visible_) first_ = selected_ - visible_ + 1;
        first_ = std::clamp(first_, 0, std::max(0, count_ - visible_));
    }
}
