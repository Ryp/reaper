////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
#// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <doctest/doctest.h>

#include "audio/alsa.h"

namespace Reaper::Audio
{
TEST_CASE("Alsa")
{
    // test_alsa();
    test_alsa2();
    // test_alsa3();
    //  test_mmap();
}
} // namespace Reaper::Audio
