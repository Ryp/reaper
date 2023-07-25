////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_FORMAT_BITFIELD_INCLUDED
#define LIB_FORMAT_BITFIELD_INCLUDED

uint bitfield_extract(uint bitfield, uint first_bit_offset, uint bit_count)
{
    uint mask = (1u << bit_count) - 1;
    return (bitfield >> first_bit_offset) & mask;
}

#endif
