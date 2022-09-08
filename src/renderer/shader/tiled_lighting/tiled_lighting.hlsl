////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef TILED_LIGHTING_INCLUDED
#define TILED_LIGHTING_INCLUDED

#include "share/tiled_lighting.hlsl"

uint get_tile_index_flat(uint2 tile_index, uint tile_count_x)
{
    return tile_index.y * tile_count_x + tile_index.x;
}

uint get_light_list_offset(uint tile_index_flat)
{
    return tile_index_flat * ElementsPerTile;
}

#endif
