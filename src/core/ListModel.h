#pragma once

namespace b21ui::core {
    // Selection and scroll window of a FO4-style item list (BSScrollingList): the selection moves
    // with D-pad / arrows / wheel and the visible window follows it; an empty list selects -1.
    class ListModel {
    public:
        void SetCount(int count);
        void SetVisibleRows(int rows);
        void Select(int index);
        // D-pad / arrows. Wrapping lists go from the last entry to the first and back.
        void Move(int delta, bool wrap);
        // A page of rows, clamped at the ends.
        void Page(int direction);

        [[nodiscard]] int Count() const { return count_; }
        [[nodiscard]] int Selected() const { return selected_; }
        [[nodiscard]] int First() const { return first_; }
        [[nodiscard]] int VisibleRows() const { return visible_; }
        [[nodiscard]] int Last() const;  // last visible index, exclusive

    private:
        void Follow();

        int count_{0};
        int visible_{1};
        int selected_{-1};
        int first_{0};
    };
}
