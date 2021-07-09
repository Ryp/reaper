////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "VectorTypes.h"

template <typename T>
inline bool operator<(const vec2<T> a, const vec2<T> b)
{
    return a.x < b.x && a.y < b.y;
}

template <typename T>
inline bool operator<=(const vec2<T> a, const vec2<T> b)
{
    return a.x <= b.x && a.y <= b.y;
}

template <typename T>
inline bool operator==(const vec2<T> a, const vec2<T> b)
{
    return a.x == b.x && a.y == b.y;
}

template <typename T>
inline bool operator!=(const vec2<T> a, const vec2<T> b)
{
    return a.x != b.x || a.y != b.y;
}

template <typename T>
inline T manhattanDistance(const vec2<T> a, const vec2<T> b)
{
    return b.x - a.x + b.y - a.y;
}
