////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <type_traits>

template <typename T>
constexpr auto to_underlying(T enumValue) noexcept
{
    static_assert(std::is_enum<T>::value, "this is only valid for enums");
    return static_cast<std::underlying_type_t<T>>(enumValue);
}
