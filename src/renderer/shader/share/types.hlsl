////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_TYPES_INCLUDED
#define SHARE_TYPES_INCLUDED

#ifdef REAPER_SHADER_CODE
    #define sl_uint     uint
    #define sl_float4x4 float4x4
#else
    using sl_uint       = u32;
    using sl_float4x4   = glm::mat4;
#endif

#endif
