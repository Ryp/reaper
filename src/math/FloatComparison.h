////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>

namespace Reaper::Math
{
constexpr float DefaultEpsilon = 1e-4f;

inline bool IsEqualWithEpsilon(float lhs, float rhs, float epsilon = DefaultEpsilon)
{
    return std::abs(rhs - lhs) < epsilon;
}
} // namespace Reaper::Math
