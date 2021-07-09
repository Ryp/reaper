////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <string>

#include "resource/Resource.h"

#include "Mesh.h"

class REAPER_MESH_API MeshCache
{
public:
    MeshCache() = default;
    ~MeshCache() = default;

public:
    void        loadMesh(MeshId id, Mesh mesh);
    const Mesh& operator[](const MeshId& id) const;
    Mesh&       operator[](const MeshId& id);

private:
    std::map<MeshId, Mesh> _meshes;
};
