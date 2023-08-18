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

struct Mesh;

namespace Reaper
{
struct MeshCache
{
    static constexpr u32 MAX_INDEX_COUNT = 1000000;
    static constexpr u32 MAX_VERTEX_COUNT = 2000000;
    static constexpr u32 MAX_MESHLET_COUNT = 4096;

    GPUBuffer vertexBufferPosition;
    GPUBuffer vertexBufferNormal;
    GPUBuffer vertexBufferUV;
    GPUBuffer indexBuffer;
    GPUBuffer meshletBuffer;

    u32 current_uv_offset;
    u32 current_normal_offset;
    u32 current_position_offset;
    u32 current_index_offset;
    u32 current_meshlet_offset;

    std::vector<Mesh2> mesh2_instances;
};

struct ReaperRoot;
struct VulkanBackend;

MeshCache create_mesh_cache(ReaperRoot& root, VulkanBackend& backend);
void      destroy_mesh_cache(VulkanBackend& backend, const MeshCache& mesh_cache);

// This invalidates all current handles
REAPER_RENDERER_API void clear_meshes(MeshCache& mesh_cache);

REAPER_RENDERER_API void load_meshes(VulkanBackend& backend, MeshCache& mesh_cache, std::span<const Mesh> meshes,
                                     std::span<MeshHandle> output_handles);
} // namespace Reaper
