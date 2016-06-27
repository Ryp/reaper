////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_CAST_INCLUDED
#define REAPER_CAST_INCLUDED

#include <type_traits>

#include "Platform.h"

template <typename CastType, typename T>
inline CastType checked_cast(T ptr)
{
    static_assert(std::is_pointer<T>::value, "input value is not a pointer");
    static_assert(std::is_pointer<CastType>::value, "cast type is not of pointer type");
#if defined(REAPER_DEBUG)
    CastType cast_ptr = dynamic_cast<CastType>(ptr);
    Assert(cast_ptr != nullptr);
    return cast_ptr;
#else
    return static_cast<T>(ptr);
#endif
}

#endif // REAPER_CAST_INCLUDED
