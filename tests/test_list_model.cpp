#include <doctest/doctest.h>
#include "core/ListModel.h"

using b21ui::core::ListModel;

TEST_CASE("an empty list selects nothing; a filled one starts at the top") {
    ListModel list;
    list.SetCount(0);
    CHECK(list.Selected() == -1);
    list.SetCount(5);
    CHECK(list.Selected() == 0);
    CHECK(list.First() == 0);
}

TEST_CASE("the visible window follows the selection both ways") {
    ListModel list;
    list.SetVisibleRows(3);
    list.SetCount(10);
    list.Select(5);
    CHECK(list.First() == 3);
    CHECK(list.Last() == 6);
    list.Select(1);
    CHECK(list.First() == 1);
}

TEST_CASE("moving wraps only when asked") {
    ListModel list;
    list.SetVisibleRows(4);
    list.SetCount(6);
    list.Move(-1, false);
    CHECK(list.Selected() == 0);
    list.Move(-1, true);
    CHECK(list.Selected() == 5);
    CHECK(list.First() == 2);
    list.Move(1, true);
    CHECK(list.Selected() == 0);
    CHECK(list.First() == 0);
}

TEST_CASE("paging moves a window of rows and clamps at the end") {
    ListModel list;
    list.SetVisibleRows(4);
    list.SetCount(10);
    list.Page(1);
    CHECK(list.Selected() == 4);
    list.Page(1);
    list.Page(1);
    CHECK(list.Selected() == 9);
    CHECK(list.First() == 6);
}

TEST_CASE("shrinking the list keeps the selection and window in range") {
    ListModel list;
    list.SetVisibleRows(3);
    list.SetCount(10);
    list.Select(9);
    list.SetCount(4);
    CHECK(list.Selected() == 3);
    CHECK(list.First() == 1);
    list.SetCount(2);
    CHECK(list.First() == 0);
}
