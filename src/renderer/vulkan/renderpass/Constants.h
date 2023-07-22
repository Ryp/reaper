////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "renderer/format/PixelFormat.h"

namespace Reaper
{
constexpr bool        MainPassUseReverseZ = true;
constexpr PixelFormat MainPassDepthFormat = PixelFormat::D16_UNORM;
} // namespace Reaper
