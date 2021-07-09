////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
#include <doctest/doctest.h>

#include "math/Spline.h"

TEST_CASE("Spline")
{
    std::vector<glm::vec4> controlPoints = {glm::vec4(0.f, 0.f, 0.f, 1.0f), glm::vec4(1.f, 0.f, 0.f, 1.0f),
                                            glm::vec4(3.f, 0.f, 0.f, 1.0f), glm::vec4(4.f, 0.f, 0.f, 1.0f)};

    Reaper::Math::Spline s = Reaper::Math::ConstructSpline(3, controlPoints);

    // TODO: Implement IsEqualEpsilon for floats
    CHECK(EvalSpline(s, 0.5f).x == 2.f);
}
