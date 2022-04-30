////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace f32_type
{
constexpr u32 MantissaBits = 23;
constexpr u32 MantissaMask = 0x7FFFFF;
constexpr u32 ExponentBits = 8;
constexpr u32 ExponentMask = 0xFF;
} // namespace f32_type

namespace f16
{
constexpr int MantissaBits = 10;
constexpr int MantissaMask = 0x3FF;
constexpr int ExponentBits = 5;
constexpr int ExponentMask = 0x1F;

float to_f32(u16 data);
} // namespace f16

namespace uf10
{
constexpr int MantissaBits = 5;
constexpr int MantissaMask = 0x1F;
constexpr int ExponentBits = 5;
constexpr int ExponentMask = 0x1F;

float to_f32(u16 data);
} // namespace uf10

namespace uf11
{
constexpr int MantissaBits = 6;
constexpr int MantissaMask = 0x3F;
constexpr int ExponentBits = 5;
constexpr int ExponentMask = 0x1F;

float to_f32(u16 data);
} // namespace uf11
