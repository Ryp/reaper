////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/PrepareBuckets.h"

// This is prototype-level code, name are extra verbose for easier refactoring.
namespace Reaper
{
using ReaperSource = const char*;
static constexpr ReaperSource EmptySource = nullptr;

struct ReaperMesh
{
    ReaperSource source = EmptySource;
    MeshHandle   handle;
};

struct ReaperGeometry
{
    ReaperMesh mesh;
};

} // namespace Reaper
