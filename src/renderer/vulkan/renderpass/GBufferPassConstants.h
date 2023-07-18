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
constexpr PixelFormat GBufferRT0Format = PixelFormat::R32_UINT;
constexpr PixelFormat GBufferRT1Format = PixelFormat::R32_UINT;
constexpr PixelFormat GBufferDepthFormat = PixelFormat::D16_UNORM;
} // namespace Reaper
