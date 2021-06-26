////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MeshCache.h"

#include "renderer/vulkan/Backend.h"
#include "renderer/vulkan/api/Vulkan.h"

#include "mesh/Mesh.h"
#include "mesh/ModelLoader.h"

#include "common/Log.h"
#include "common/ReaperRoot.h"

#include <meshoptimizer.h>

#include "renderer/shader/share/culling.hlsl"

namespace Reaper
{
MeshCache create_mesh_cache(ReaperRoot& root, VulkanBackend& backend)
{
    MeshCache cache;

    cache.vertexBufferPosition =
        create_buffer(root, backend.device, "Position buffer",
                      DefaultGPUBufferProperties(MeshCache::MAX_VERTEX_COUNT, 3 * sizeof(float),
                                                 GPUBufferUsage::VertexBuffer | GPUBufferUsage::StorageBuffer),
                      backend.vma_instance);
    cache.vertexBufferNormal = create_buffer(
        root, backend.device, "Normal buffer",
        DefaultGPUBufferProperties(MeshCache::MAX_VERTEX_COUNT, 3 * sizeof(float), GPUBufferUsage::VertexBuffer),
        backend.vma_instance);
    cache.vertexBufferUV = create_buffer(
        root, backend.device, "UV buffer",
        DefaultGPUBufferProperties(MeshCache::MAX_VERTEX_COUNT, 2 * sizeof(float), GPUBufferUsage::VertexBuffer),
        backend.vma_instance);
    cache.indexBuffer = create_buffer(
        root, backend.device, "Index buffer",
        DefaultGPUBufferProperties(MeshCache::MAX_INDEX_COUNT, sizeof(int), GPUBufferUsage::StorageBuffer),
        backend.vma_instance);

    cache.current_uv_offset = 0;
    cache.current_normal_offset = 0;
    cache.current_position_offset = 0;
    cache.current_index_offset = 0;

    return cache;
}

void destroy_mesh_cache(VulkanBackend& backend, const MeshCache& mesh_cache)
{
    vmaDestroyBuffer(backend.vma_instance, mesh_cache.indexBuffer.buffer, mesh_cache.indexBuffer.allocation);
    vmaDestroyBuffer(backend.vma_instance, mesh_cache.vertexBufferPosition.buffer,
                     mesh_cache.vertexBufferPosition.allocation);
    vmaDestroyBuffer(backend.vma_instance, mesh_cache.vertexBufferNormal.buffer,
                     mesh_cache.vertexBufferNormal.allocation);
    vmaDestroyBuffer(backend.vma_instance, mesh_cache.vertexBufferUV.buffer, mesh_cache.vertexBufferUV.allocation);
}

namespace
{
    MeshAlloc mesh_cache_allocate_mesh(MeshCache& mesh_cache, const Mesh& mesh)
    {
        MeshAlloc alloc = {};

        const u32 index_count = mesh.indexes.size();
        const u32 position_count = mesh.vertices.size();
        const u32 normal_count = mesh.normals.size();
        const u32 uv_count = mesh.uvs.size();

        Assert(index_count > 0);
        Assert(position_count > 0);
        Assert(normal_count == position_count);
        Assert(uv_count == position_count);

        alloc.index_count = index_count;
        alloc.vertex_count = position_count;

        alloc.index_offset = mesh_cache.current_index_offset;
        alloc.position_offset = mesh_cache.current_position_offset;
        alloc.normal_offset = mesh_cache.current_normal_offset;
        alloc.uv_offset = mesh_cache.current_uv_offset;

        mesh_cache.current_index_offset += index_count;
        mesh_cache.current_position_offset += position_count;
        mesh_cache.current_normal_offset += normal_count;
        mesh_cache.current_uv_offset += uv_count;

        Assert(mesh_cache.current_index_offset < MeshCache::MAX_INDEX_COUNT);
        Assert(mesh_cache.current_position_offset < MeshCache::MAX_VERTEX_COUNT);

        return alloc;
    }

    void upload_mesh_to_mesh_cache(MeshCache& mesh_cache, const Mesh& mesh, const MeshAlloc& mesh_alloc,
                                   VulkanBackend& backend)
    {
        upload_buffer_data(backend.device, backend.vma_instance, mesh_cache.indexBuffer, mesh.indexes.data(),
                           mesh.indexes.size() * sizeof(mesh.indexes[0]), mesh_alloc.index_offset);
        upload_buffer_data(backend.device, backend.vma_instance, mesh_cache.vertexBufferPosition, mesh.vertices.data(),
                           mesh.vertices.size() * sizeof(mesh.vertices[0]), mesh_alloc.position_offset);
        upload_buffer_data(backend.device, backend.vma_instance, mesh_cache.vertexBufferNormal, mesh.normals.data(),
                           mesh.normals.size() * sizeof(mesh.normals[0]), mesh_alloc.normal_offset);
        upload_buffer_data(backend.device, backend.vma_instance, mesh_cache.vertexBufferUV, mesh.uvs.data(),
                           mesh.uvs.size() * sizeof(mesh.uvs[0]), mesh_alloc.uv_offset);
    }
} // namespace

void load_meshes(VulkanBackend& backend, MeshCache& mesh_cache, nonstd::span<const char*> mesh_filenames,
                 nonstd::span<MeshHandle> output_handles)
{
    Assert(output_handles.size() >= mesh_filenames.size());

    for (u32 file_index = 0; file_index < mesh_filenames.size(); file_index++)
    {
        Mesh mesh = ModelLoader::loadOBJ(mesh_filenames[file_index]);

        // FIXME Filling empty buffers with garbage to preserve correct index offsets
        // This can be removed if we do vertex pulling with per-buffer offsets
        if (mesh.normals.empty())
            mesh.normals.resize(mesh.vertices.size());

        if (mesh.uvs.empty())
            mesh.uvs.resize(mesh.vertices.size());

        const size_t max_vertices = ComputeCullingGroupSize / 2;
        const size_t max_triangles = ComputeCullingGroupSize;
        const float  cone_weight = 0.0f;

        Assert(max_vertices < 256); // NOTE: u8 indices

        size_t max_meshlets = meshopt_buildMeshletsBound(mesh.indexes.size(), max_vertices, max_triangles);

        std::vector<meshopt_Meshlet> meshlets(max_meshlets);
        std::vector<u32>             meshlet_vertices(max_meshlets * max_vertices);
        std::vector<u8>              meshlet_triangles(max_meshlets * max_triangles * 3);

        size_t meshlet_count = meshopt_buildMeshlets(
            meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), mesh.indexes.data(),
            mesh.indexes.size(), reinterpret_cast<const float*>(mesh.vertices.data()), mesh.vertices.size(),
            sizeof(mesh.vertices[0]), max_vertices, max_triangles, cone_weight);

        const meshopt_Meshlet& last = meshlets[meshlet_count - 1];

        meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
        meshlet_triangles.resize(last.triangle_offset + last.triangle_count * 3);
        meshlets.resize(meshlet_count);

        // Rewrite index buffer
        // TODO Also rebuilt vertex buffer since it was optimized for cache locality
        mesh.indexes.clear();

        const u32 meshlet_vertex_count = meshlet_vertices.size();
        const u32 total_mesh_index_count = meshlet_triangles.size();

        std::vector<u32>             optimized_index_buffer(total_mesh_index_count);
        std::vector<glm::fvec3>      optimized_vertex_buffer(meshlet_vertex_count);
        std::vector<glm::fvec3>      optimized_normal_buffer(meshlet_vertex_count);
        std::vector<glm::fvec2>      optimized_uv_buffer(meshlet_vertex_count);
        std::vector<MeshletInstance> optimized_meshlets(meshlet_count);

        for (u32 i = 0; i < meshlet_vertex_count; i++)
        {
            const u32 index = meshlet_vertices[i];

            optimized_vertex_buffer[i] = mesh.vertices[index];
            optimized_normal_buffer[i] = mesh.normals[index];
            optimized_uv_buffer[i] = mesh.uvs[index];
        }

        u32 index_output_offset = 0;

        // We also do index buffer compaction in the same pass
        for (meshopt_Meshlet& meshlet : meshlets)
        {
            const u32 meshlet_index_count = meshlet.triangle_count * 3;

            for (u32 meshlet_index = 0; meshlet_index < meshlet_index_count; meshlet_index++)
            {
                const u32 index_input_offset = meshlet.triangle_offset + meshlet_index;
                const u32 local_index = static_cast<u32>(meshlet_triangles[index_input_offset]);

                optimized_index_buffer[index_output_offset + meshlet_index] =
                    meshlet.vertex_offset + local_index; // FIXME
            }

            MeshletInstance& meshlet_instance = optimized_meshlets.emplace_back();
            meshlet_instance.vertex_offset = meshlet.vertex_offset;
            meshlet_instance.vertex_count = meshlet.vertex_count;
            meshlet_instance.index_offset = index_output_offset;
            meshlet_instance.index_count = meshlet_index_count;

            index_output_offset += meshlet_index_count;
        }

        optimized_index_buffer.resize(index_output_offset); // Trim excess

        std::swap(mesh.indexes, optimized_index_buffer);
        std::swap(mesh.vertices, optimized_vertex_buffer);
        std::swap(mesh.normals, optimized_normal_buffer);
        std::swap(mesh.uvs, optimized_uv_buffer);

        const MeshHandle new_handle = static_cast<MeshHandle>(mesh_cache.mesh2_instances.size());
        Mesh2&           mesh2 = mesh_cache.mesh2_instances.emplace_back();

        mesh2 = create_mesh2(mesh_cache_allocate_mesh(mesh_cache, mesh));

        std::swap(mesh2.meshlets, optimized_meshlets);

        upload_mesh_to_mesh_cache(mesh_cache, mesh, mesh2.lods_allocs[0], backend);

        output_handles[file_index] = new_handle;
    }
}
} // namespace Reaper
