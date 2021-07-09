////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
inline u32 div_round_up(u32 batch_size, u32 group_size)
{
    return (batch_size + group_size - 1) / group_size;
}
} // namespace Reaper
