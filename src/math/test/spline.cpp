////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "math/Spline.h"

TEST_CASE("Spline")
{
    Spline s(3);

    s.addControlPoint(glm::vec3(0.f, 0.f, 0.f));
    s.addControlPoint(glm::vec3(1.f, 0.f, 0.f));
    s.addControlPoint(glm::vec3(3.f, 0.f, 0.f));
    s.addControlPoint(glm::vec3(4.f, 0.f, 0.f));
    s.build();

    // TODO: Implement IsEqualEpsilon for floats
    CHECK(s.eval(0.5f).x == 2.f);
}
