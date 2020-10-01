////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
constexpr u32 MaxIndirectDrawCount = 2000;
constexpr u32 MaxCullPassCount = 4;
constexpr u32 IndirectDrawCountCount = 2; // Second uint is for keeping track of total triangles
constexpr u32 CullInstanceCountMax = 512;
constexpr u32 DynamicIndexBufferSize = static_cast<u32>(2000_kiB);
constexpr u32 IndexSizeBytes = 4;
} // namespace Reaper
