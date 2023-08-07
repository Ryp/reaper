////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_MORTON_INCLUDED
#define LIB_MORTON_INCLUDED

////////////////////////////////////////////////////////////////////////////////
// Implementation details
//
// Source:
// https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
//
// "Insert" a 0 bit after each of the 16 low bits of x
uint morton_part_1_by_1(uint x)
{
    x &= 0x0000ffff;                  // x = ---- ---- ---- ---- fedc ba98 7654 3210
    x = (x ^ (x <<  8)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = (x ^ (x <<  4)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = (x ^ (x <<  2)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = (x ^ (x <<  1)) & 0x55555555; // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0

    return x;
}

// "Insert" two 0 bits after each of the 10 low bits of x
uint morton_part_1_by_2(uint x)
{
    x &= 0x000003ff;                  // x = ---- ---- ---- ---- ---- --98 7654 3210
    x = (x ^ (x << 16)) & 0xff0000ff; // x = ---- --98 ---- ---- ---- ---- 7654 3210
    x = (x ^ (x <<  8)) & 0x0300f00f; // x = ---- --98 ---- ---- 7654 ---- ---- 3210
    x = (x ^ (x <<  4)) & 0x030c30c3; // x = ---- --98 ---- 76-- --54 ---- 32-- --10
    x = (x ^ (x <<  2)) & 0x09249249; // x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0

    return x;
}

// Inverse of morton_part_1_by_1 - "delete" all odd-indexed bits
uint morton_compact_1_by_1(uint x)
{
    x &= 0x55555555;                  // x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
    x = (x ^ (x >>  1)) & 0x33333333; // x = --fe --dc --ba --98 --76 --54 --32 --10
    x = (x ^ (x >>  2)) & 0x0f0f0f0f; // x = ---- fedc ---- ba98 ---- 7654 ---- 3210
    x = (x ^ (x >>  4)) & 0x00ff00ff; // x = ---- ---- fedc ba98 ---- ---- 7654 3210
    x = (x ^ (x >>  8)) & 0x0000ffff; // x = ---- ---- ---- ---- fedc ba98 7654 3210

    return x;
}

// Inverse of morton_part_1_by_2 - "delete" all bits not at positions divisible by 3
uint morton_compact_1_by_2(uint x)
{
    x &= 0x09249249;                  // x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
    x = (x ^ (x >>  2)) & 0x030c30c3; // x = ---- --98 ---- 76-- --54 ---- 32-- --10
    x = (x ^ (x >>  4)) & 0x0300f00f; // x = ---- --98 ---- ---- 7654 ---- ---- 3210
    x = (x ^ (x >>  8)) & 0xff0000ff; // x = ---- --98 ---- ---- ---- ---- 7654 3210
    x = (x ^ (x >> 16)) & 0x000003ff; // x = ---- ---- ---- ---- ---- --98 7654 3210

    return x;
}

////////////////////////////////////////////////////////////////////////////////
// 2D Morton
uint encode_morton_2d(uint2 v)
{
    return morton_part_1_by_1(v.x) +
          (morton_part_1_by_1(v.y) << 1);
}

uint2 decode_morton_2d(uint code)
{
    return uint2(morton_compact_1_by_1(code >> 0),
                 morton_compact_1_by_1(code >> 1));
}

////////////////////////////////////////////////////////////////////////////////
// 3D Morton
uint encode_morton_3d(uint3 v)
{
    return morton_part_1_by_2(v.x) +
          (morton_part_1_by_2(v.y) << 1) +
          (morton_part_1_by_2(v.z) << 2);
}

uint3 decode_morton_3d(uint code)
{
    return uint3(morton_compact_1_by_2(code >> 0),
                 morton_compact_1_by_2(code >> 1),
                 morton_compact_1_by_2(code >> 2));
}

#endif
