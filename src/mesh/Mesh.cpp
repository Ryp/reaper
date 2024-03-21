////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2024 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Mesh.h"

namespace Reaper
{
Mesh duplicate_mesh(const Mesh& mesh)
{
    Mesh dup;
    dup.indexes = mesh.indexes;
    dup.positions = mesh.positions;
    dup.normals = mesh.normals;
    dup.tangents = mesh.tangents;
    dup.uvs = mesh.uvs;
    return dup;
}

} // namespace Reaper
