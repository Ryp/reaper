////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <core/Types.h>

namespace Reaper
{
template <typename HandleType>
struct HandleSpan
{
    u32 offset;
    u32 count;
};

enum MeshHandle : u32
{
};
static constexpr MeshHandle InvalidMeshHandle = MeshHandle(0xFFFFFFFF);

enum TextureHandle : u32
{
};
static constexpr TextureHandle InvalidTextureHandle = TextureHandle(0xFFFFFFFF);
} // namespace Reaper
