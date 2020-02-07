////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef SHARE_TYPES_INCLUDED
#define SHARE_TYPES_INCLUDED

#ifdef REAPER_SHADER_CODE
    #define sl_uint uint
#else
    using sl_uint = u32;
#endif

#endif
