////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2020 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/format/PixelFormat.h"

namespace Reaper
{
constexpr bool        ShadowUseReverseZ = true;
constexpr u32         ShadowMapResolution = 1024;
constexpr u32         ShadowMapMaxCount = 8;
constexpr PixelFormat ShadowMapFormat = PixelFormat::D16_UNORM;
} // namespace Reaper
