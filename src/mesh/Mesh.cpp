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
    dup.attributes = mesh.attributes;
    return dup;
}

} // namespace Reaper
