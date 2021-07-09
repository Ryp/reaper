////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

inline void toggle(bool& value)
{
    value = !value;
}

// Return integer with bit n set to 1
constexpr u32 bit(u32 n)
{
    return 1 << n;
}

// Note: 0 will return true
template <typename T>
constexpr bool isPowerOfTwo(T value)
{
    static_assert(std::is_unsigned<T>::value, "non-unsigned type");
    return ((value & (value - 1)) == 0);
}

// Brian Kernighan's way
inline u32 countBits(u32 value)
{
    unsigned int count;

    for (count = 0; value > 0; count++)
        value &= value - 1; // clear the least significant bit set

    return count;
}

// bitOffset(0) returns 0
template <typename T>
inline u32 bitOffset(T value)
{
    u32 count;

    static_assert(std::is_unsigned<T>::value, "non-unsigned type");
    for (count = 0; value > 0; count++)
        value >>= 1;

    return count - 1;
}
