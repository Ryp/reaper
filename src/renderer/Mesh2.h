////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

namespace Reaper
{
struct MeshAlloc
{
    u32 index_count;
    u32 vertex_count;

    // Buffer offsets
    u32 index_offset;
    u32 position_offset;
    u32 uv_offset;
    u32 normal_offset;
};

struct MeshletInstance
{
    u32 index_offset;
    u32 index_count;
    u32 vertex_offset;
    u32 vertex_count;
};

struct Mesh2
{
    u32 lod_count;

    std::vector<MeshletInstance> meshlets;

    static constexpr u32 MAX_MESH_LODS = 4;
    MeshAlloc            lods_allocs[MAX_MESH_LODS];
};

inline Mesh2 create_mesh2(MeshAlloc alloc)
{
    return {
        1,
        {},
        {alloc, {}, {}, {}},
    };
}
} // namespace Reaper
