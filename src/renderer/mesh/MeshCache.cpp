////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MeshCache.h"

void MeshCache::loadMesh(MeshId id, Mesh mesh)
{
    Assert(_meshes.count(id) == 0, "mesh already in cache");
    _meshes.emplace(id, mesh);
}
