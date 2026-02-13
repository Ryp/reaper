////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Buffer.h"

#include <span>
#include <vector>

#include "renderer/Mesh2.h"
#include "renderer/RendererExport.h"
#include "renderer/ResourceHandle.h"

namespace Reaper
{
struct Mesh;
struct MeshCache
{
    static constexpr u32 MAX_INDEX_COUNT = 2000000;
    static constexpr u32 MAX_VERTEX_COUNT = 4000000;
    static constexpr u32 MAX_MESHLET_COUNT = MAX_VERTEX_COUNT / 64; // FIXME

    GPUBuffer indexBuffer;
    GPUBuffer vertexBufferPosition;
    GPUBuffer vertexAttributesBuffer;
    GPUBuffer meshletBuffer;

    u32 current_index_offset;
    u32 current_position_offset;
    u32 current_attributes_offset;
    u32 current_meshlet_offset;

    std::vector<Mesh2> mesh2_instances;
};

struct VulkanBackend;

MeshCache create_mesh_cache(VulkanBackend& backend);
void      destroy_mesh_cache(VulkanBackend& backend, const MeshCache& mesh_cache);

// This invalidates all current handles
REAPER_RENDERER_API void clear_meshes(MeshCache& mesh_cache);

REAPER_RENDERER_API void load_meshes(VulkanBackend& backend, MeshCache& mesh_cache, std::span<const Mesh> meshes,
                                     std::span<MeshHandle> output_handles);
} // namespace Reaper
