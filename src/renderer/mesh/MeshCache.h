////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <string>

#include "resource/Resource.h"

#include "Mesh.h"

class MeshCache
{
public:
    MeshCache() = default;
    ~MeshCache() = default;

public:
    void loadMesh(MeshId id, Mesh mesh);
    const Mesh& operator[](const MeshId& id) const;

private:
    std::map<MeshId, Mesh> _meshes;
};
