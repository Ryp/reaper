////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/format/PixelFormat.h"

namespace Reaper
{
constexpr bool        ShadowUseReverseZ = true;
constexpr PixelFormat ShadowMapFormat = PixelFormat::D16_UNORM;
} // namespace Reaper
