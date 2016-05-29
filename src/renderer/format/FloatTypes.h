////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_FLOATTYPES_INCLUDED
#define REAPER_FLOATTYPES_INCLUDED

namespace half
{
    float toFloat(u16 data);
}

namespace uf10
{
    float toFloat(u16 data);
}

namespace uf11
{
    float toFloat(u16 data);
}

#endif // REAPER_FLOATTYPES_INCLUDED
