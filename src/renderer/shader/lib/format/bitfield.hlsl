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

uint merge_uint_4x8_to_32(uint4 uint_4x8)
{
    uint uint_32 = uint_4x8.x
                + (uint_4x8.y << 8)
                + (uint_4x8.z << 16)
                + (uint_4x8.w << 24);

    return uint_32;
}

uint3 split_uint_32_to_3x8(uint uint_32)
{
    uint3 uint_3x8 = uint3(uint_32,
                           uint_32 >> 8,
                           uint_32 >> 16);

    return uint_3x8 & 0xFF;
}

uint4 split_uint_32_to_4x8(uint uint_32)
{
    uint4 uint_4x8 = uint4(uint_32,
                           uint_32 >> 8,
                           uint_32 >> 16,
                           uint_32 >> 24);

    return uint_4x8 & 0xFF;
}

#endif
