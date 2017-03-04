////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_VECTORTYPES_INCLUDED
#define REAPER_VECTORTYPES_INCLUDED

template <typename T>
struct vec2 {
    using vtype = T;
    vtype x;
    vtype y;
    vec2() : x(0), y(0) {}
    vec2(vtype X, vtype Y) : x(X), y(Y) {}
};

using uvec2 = vec2<unsigned int>;
using u8vec2 = vec2<u8>;

#include "VectorTypes.inl"

#endif // REAPER_VECTORTYPES_INCLUDED
