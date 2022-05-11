////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
constexpr u32 MaxIndirectDrawCount = 8000;
constexpr u32 MaxCullPassCount = 4;
constexpr u32 CullInstanceCountMax = 2000;
constexpr u32 DynamicMeshletBufferElements = 8096;
constexpr u32 DynamicIndexBufferSizeBytes = static_cast<u32>(32_MiB);
constexpr u32 IndexSizeBytes = 4;
} // namespace Reaper
