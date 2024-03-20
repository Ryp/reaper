////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2024 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
enum class TransferFunction
{
    Unknown,
    Linear,
    sRGB,
    Rec709,
    PQ,
    // FIXME Add HLG but assert
    scRGB_Windows,
};
} // namespace Reaper
