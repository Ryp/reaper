////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MeshCache.h"

void MeshCache::loadMesh(MeshId id, Mesh mesh)
{
    Assert(_meshes.count(id) == 0, "mesh already in cache");
    _meshes.emplace(id, mesh);
}

const Mesh& MeshCache::operator[](const MeshId& id) const
{
    Assert(_meshes.count(id) > 0, "mesh missing from cache");
    return _meshes.at(id);
}

Mesh& MeshCache::operator[](const MeshId& id)
{
    Assert(_meshes.count(id) > 0, "mesh missing from cache");
    return _meshes.at(id);
}
