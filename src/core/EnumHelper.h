////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_ENUMHELPER_INCLUDED
#define REAPER_ENUMHELPER_INCLUDED

#include <type_traits>

template <typename T>
constexpr auto to_underlying(T enumValue) noexcept
{
    static_assert(std::is_enum<T>(), "this is only valid for enums");
    return static_cast<std::underlying_type_t<T>>(enumValue);
}

#endif // REAPER_ENUMHELPER_INCLUDED
