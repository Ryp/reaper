////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_CULLING_INCLUDED
#define SHARE_CULLING_INCLUDED

#include "types.hlsl"

struct CullPushConstants
{
    sl_uint triangleCount;
    sl_uint firstIndex;
};

#endif
