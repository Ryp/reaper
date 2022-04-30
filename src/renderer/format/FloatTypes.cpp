////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FloatTypes.h"

// TODO Fix possible overflows
// TODO Handle endianness
// TODO Unit test

namespace
{
union Float32
{
    float as_float;
    struct
    {
        u32 mantissa : f32_type::MantissaBits;
        u32 exponent : f32_type::ExponentBits;
        u32 sign : 1;
    };
};

static_assert(sizeof(Float32) == sizeof(u32), "malformed bitfields");
} // namespace

namespace f16
{
namespace
{
    union HalfFloat
    {
        u16 as_u16;
        struct
        {
            u16 mantissa : MantissaBits;
            u16 exponent : ExponentBits;
            u16 sign : 1;
        };
    };

    static_assert(sizeof(HalfFloat) == sizeof(u16), "malformed bitfields");

    inline Float32 HalfToFloatImpl(HalfFloat in)
    {
        Float32 out = {0};

        out.sign = in.sign;
        if (in.exponent != 0 || in.mantissa != 0)
        {
            if (in.exponent == 0) // Denormal
            {
                i32 e = -1;
                u32 m = in.mantissa;
                do
                {
                    e++;
                    m <<= 1;
                } while ((m & 0x400) == 0);

                out.mantissa = (m & MantissaMask) << (f32_type::MantissaBits - MantissaBits);
                out.exponent = (f32_type::ExponentMask >> 1) - (ExponentMask >> 1) - e;
            }
            else if (in.exponent == ExponentMask) // Inf or NaN
            {
                out.mantissa = in.mantissa << (f32_type::MantissaBits - MantissaBits);
                out.exponent = f32_type::ExponentMask;
            }
            else // Normalized
            {
                out.mantissa = in.mantissa << (f32_type::MantissaBits - MantissaBits);
                out.exponent = (f32_type::ExponentMask >> 1) - (ExponentMask >> 1) + in.exponent;
            }
        }
        return out;
    }
} // namespace

float to_f32(u16 data)
{
    HalfFloat in;
    in.as_u16 = data;

    Float32 out = HalfToFloatImpl(in);
    return out.as_float;
}
} // namespace f16

namespace uf10
{
namespace
{
    union Uf10
    {
        u16 as_u16;
        struct
        {
            u16 mantissa : MantissaBits;
            u16 exponent : ExponentBits;
            u16 : 6;
        };
    };

    static_assert(sizeof(Uf10) == sizeof(u16), "malformed bitfields");

    inline Float32 Uf10ToFloatImpl(Uf10 in)
    {
        Float32 out = {0};

        if (in.exponent != 0 || in.mantissa != 0)
        {
            if (in.exponent == 0) // Denormal
            {
                // Leave as is for now
                out.mantissa = in.mantissa << (f32_type::MantissaBits - MantissaBits);
                out.exponent = (f32_type::ExponentMask >> 1) - (ExponentMask >> 1) + in.exponent;
            }
            else if (in.exponent == ExponentMask) // Inf or NaN
            {
                out.mantissa = in.mantissa << (f32_type::MantissaBits - MantissaBits);
                out.exponent = f32_type::ExponentMask;
            }
            else // Normalized
            {
                out.mantissa = in.mantissa << (f32_type::MantissaBits - MantissaBits);
                out.exponent = (f32_type::ExponentMask >> 1) - (ExponentMask >> 1) + in.exponent;
            }
        }
        return out;
    }
} // namespace

float to_f32(u16 data)
{
    Uf10 in;
    in.as_u16 = data;

    Float32 out = Uf10ToFloatImpl(in);
    return out.as_float;
}
} // namespace uf10

namespace uf11
{
namespace
{
    union Uf11
    {
        u16 as_u16;
        struct
        {
            u16 mantissa : MantissaBits;
            u16 exponent : ExponentBits;
            u16 : 5;
        };
    };

    static_assert(sizeof(Uf11) == sizeof(u16), "malformed bitfields");

    inline Float32 Uf11ToFloatImpl(Uf11 in)
    {
        Float32 out = {0};

        if (in.exponent != 0 || in.mantissa != 0)
        {
            if (in.exponent == 0) // Denormal
            {
                // Leave as is for now
                out.mantissa = in.mantissa << (f32_type::MantissaBits - MantissaBits);
                out.exponent = (f32_type::ExponentMask >> 1) - (ExponentMask >> 1) + in.exponent;
            }
            else if (in.exponent == ExponentMask) // Inf or NaN
            {
                out.mantissa = in.mantissa << (f32_type::MantissaBits - MantissaBits);
                out.exponent = f32_type::ExponentMask;
            }
            else // Normalized
            {
                out.mantissa = in.mantissa << (f32_type::MantissaBits - MantissaBits);
                out.exponent = (f32_type::ExponentMask >> 1) - (ExponentMask >> 1) + in.exponent;
            }
        }
        return out;
    }
} // namespace

float to_f32(u16 data)
{
    Uf11 in;
    in.as_u16 = data;

    Float32 out = Uf11ToFloatImpl(in);
    return out.as_float;
}
} // namespace uf11
