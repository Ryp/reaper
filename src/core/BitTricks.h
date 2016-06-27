////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_BITTRICKS_INCLUDED
#define REAPER_BITTRICKS_INCLUDED

// Return integer with bit n set to 1
constexpr unsigned int bit(unsigned int n)
{
    return 1 << n;
}

// Note: 0 will return true
constexpr bool isPowerOfTwo(unsigned int value)
{
    return ((value & (value - 1)) == 0);
}

// Brian Kernighan's way
inline unsigned int countBits(unsigned int value)
{
    unsigned int count;

    for (count = 0; value > 0; count++)
        value &= value - 1; // clear the least significant bit set

    return count;
}

// bitOffset(0) returns 0
inline unsigned int bitOffset(unsigned int value)
{
    unsigned int count;

    for (count = 0; value > 0; count++)
        value >>= 1;

    return count - 1;
}

#endif // REAPER_BITTRICKS_INCLUDED
