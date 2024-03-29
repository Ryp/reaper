////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <core/Types.h>
#include <math/MathExport.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <list>
#include <span>
#include <vector>

namespace Reaper::Math
{
struct Spline
{
    std::vector<glm::vec4> controlPoints; // w stores the weight
    std::vector<float>     knots;
    u32                    order;
};

REAPER_MATH_API Spline create_spline(u32 order, std::span<const glm::vec4> controlPoints);

REAPER_MATH_API glm::vec3 spline_eval(const Spline& spline, float t);
} // namespace Reaper::Math
