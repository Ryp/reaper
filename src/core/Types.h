////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

/* Integer */
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

/* Floating point */
using f32 = float;
using f64 = double;

static_assert(sizeof(f32) == 4, "floats are not 32 bits");
static_assert(sizeof(f64) == 8, "doubles are not 64 bits");
