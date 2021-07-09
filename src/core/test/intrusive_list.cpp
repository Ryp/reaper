////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "core/IntrusiveList.h"

struct Point : public Reaper::Link<Point>
{
    int a;
    int b;
};

TEST_CASE("Intrusive list")
{
    Point p1;
    Point p2;

    p1.a = 3;
    p1.b = 4;

    p2.a = 8;
    p2.b = 9;

    Reaper::List<Point> list;

    CHECK_EQ(list.Size(), 0);
    CHECK(list.Empty());

    list.Insert(&p1);
    list.Insert(&p2);

    CHECK_EQ(list.Size(), 2);
    CHECK(!list.Empty());

    list.Remove(&p2);

    CHECK_EQ(list.Size(), 1);
    CHECK(!list.Empty());

    list.Remove(&p1);

    CHECK_EQ(list.Size(), 0);
    CHECK(list.Empty());
}
