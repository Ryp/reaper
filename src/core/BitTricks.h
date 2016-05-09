////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
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

#endif // REAPER_BITTRICKS_INCLUDED
