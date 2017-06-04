////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_MESHCACHE_INCLUDED
#define REAPER_MESHCACHE_INCLUDED

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

#endif // REAPER_MESHCACHE_INCLUDED
