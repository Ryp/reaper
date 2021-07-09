////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>

#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>

template <typename T>
constexpr T sqr(T value)
{
    return value * value;
}

// Arithmetic operations handling unsigned types overflow
template <typename T>
inline T decrement(T value, T amount, T min = 0)
{
    static_assert(std::is_unsigned<T>::value, "non-unsigned type");
    if (value > amount + min)
        return value - amount;
    else
        return min;
}

template <typename T>
inline T increment(T value, T amount, T max = -1)
{
    static_assert(std::is_unsigned<T>::value, "non-unsigned type");
    if (value < max - amount)
        return value + amount;
    else
        return max;
}

// Return smallest signed angle from a to b in range [-pi;pi]
inline float deltaAngle2D(const glm::vec2 a, const glm::vec2 b)
{
    const float delta = atan2f(b.y, b.x) - atan2f(a.y, a.x);

    if (delta > glm::pi<float>())
        return delta - 2.f * glm::pi<float>();
    else if (delta < -glm::pi<float>())
        return delta + 2.f * glm::pi<float>();
    return delta;
}

// Return angle from a vector in range [-pi;pi]
inline float angle(const glm::vec2 v)
{
    return atan2f(v.y, v.x);
}

inline float lengthSq(const glm::vec2 v)
{
    return v.x * v.x + v.y * v.y;
}

inline float distanceSq(const glm::vec2 a, const glm::vec2 b)
{
    return lengthSq(b - a);
}
