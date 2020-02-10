////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2019 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace Reaper
{
inline u32 group_count(u32 batch_size, u32 group_size)
{
    Assert(batch_size > 0);

    return (batch_size + group_size - 1) / group_size;
}
} // namespace Reaper
