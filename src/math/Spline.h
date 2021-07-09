////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <list>
#include <vector>

namespace Reaper::Math
{
struct Spline
{
    std::vector<glm::vec4> controlPoints; // w stores the weight
    std::vector<float>     knots;
    u32                    order;
};

REAPER_MATH_API Spline ConstructSpline(u32 order, const std::vector<glm::vec4>& controlPoints);

REAPER_MATH_API glm::vec3 EvalSpline(const Spline& spline, float t);
} // namespace Reaper::Math
