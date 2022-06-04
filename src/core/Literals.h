////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdlib>

namespace Reaper
{
// User-defined literals
// 1 kiB = 1024 bytes
constexpr std::size_t operator"" _kiB(unsigned long long int sizeInkiB)
{
    return sizeInkiB << 10;
}

constexpr std::size_t operator"" _MiB(unsigned long long int sizeInMiB)
{
    return sizeInMiB << 20;
}

constexpr std::size_t operator"" _GiB(unsigned long long int sizeInGiB)
{
    return sizeInGiB << 30;
}
} // namespace Reaper
