////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "FloatTypes.h"

// TODO Fix possible overflows
// TODO Handle endianness
// TODO Unit test

namespace
{
constexpr int Float32MantissaBits = 23;
constexpr int Float32MantissaMask = 0x7FFFFF;
constexpr int Float32ExponentBits = 8;
constexpr int Float32ExponentMask = 0xFF;

union Float32
{
    float as_float;
    struct
    {
        u32 mantissa : Float32MantissaBits;
        u32 exponent : Float32ExponentBits;
        u32 sign : 1;
    };
};

static_assert(sizeof(Float32) == sizeof(u32), "malformed bitfields");
} // namespace

namespace half
{
constexpr int HalfMantissaBits = 10;
constexpr int HalfMantissaMask = 0x3FF;
constexpr int HalfExponentBits = 5;
constexpr int HalfExponentMask = 0x1F;

namespace
{
    union HalfFloat
    {
        u16 as_u16;
        struct
        {
            u16 mantissa : HalfMantissaBits;
            u16 exponent : HalfExponentBits;
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

                out.mantissa = (m & HalfMantissaMask) << (Float32MantissaBits - HalfMantissaBits);
                out.exponent = (Float32ExponentMask >> 1) - (HalfExponentMask >> 1) - e;
            }
            else if (in.exponent == HalfExponentMask) // Inf or NaN
            {
                out.mantissa = in.mantissa << (Float32MantissaBits - HalfMantissaBits);
                out.exponent = Float32ExponentMask;
            }
            else // Normalized
            {
                out.mantissa = in.mantissa << (Float32MantissaBits - HalfMantissaBits);
                out.exponent = (Float32ExponentMask >> 1) - (HalfExponentMask >> 1) + in.exponent;
            }
        }
        return out;
    }
} // namespace

float toFloat(u16 data)
{
    HalfFloat in;
    in.as_u16 = data;

    Float32 out = HalfToFloatImpl(in);
    return out.as_float;
}
} // namespace half

namespace uf10
{
constexpr int Uf10MantissaBits = 5;
constexpr int Uf10MantissaMask = 0x1F;
constexpr int Uf10ExponentBits = 5;
constexpr int Uf10ExponentMask = 0x1F;

namespace
{
    union Uf10
    {
        u16 as_u16;
        struct
        {
            u16 mantissa : Uf10MantissaBits;
            u16 exponent : Uf10ExponentBits;
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
                out.mantissa = in.mantissa << (Float32MantissaBits - Uf10MantissaBits);
                out.exponent = (Float32ExponentMask >> 1) - (Uf10ExponentMask >> 1) + in.exponent;
            }
            else if (in.exponent == Uf10ExponentMask) // Inf or NaN
            {
                out.mantissa = in.mantissa << (Float32MantissaBits - Uf10MantissaBits);
                out.exponent = Float32ExponentMask;
            }
            else // Normalized
            {
                out.mantissa = in.mantissa << (Float32MantissaBits - Uf10MantissaBits);
                out.exponent = (Float32ExponentMask >> 1) - (Uf10ExponentMask >> 1) + in.exponent;
            }
        }
        return out;
    }
} // namespace

float toFloat(u16 data)
{
    Uf10 in;
    in.as_u16 = data;

    Float32 out = Uf10ToFloatImpl(in);
    return out.as_float;
}
} // namespace uf10

namespace uf11
{
constexpr int Uf11MantissaBits = 6;
constexpr int Uf11MantissaMask = 0x3F;
constexpr int Uf11ExponentBits = 5;
constexpr int Uf11ExponentMask = 0x1F;

namespace
{
    union Uf11
    {
        u16 as_u16;
        struct
        {
            u16 mantissa : Uf11MantissaBits;
            u16 exponent : Uf11ExponentBits;
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
                out.mantissa = in.mantissa << (Float32MantissaBits - Uf11MantissaBits);
                out.exponent = (Float32ExponentMask >> 1) - (Uf11ExponentMask >> 1) + in.exponent;
            }
            else if (in.exponent == Uf11ExponentMask) // Inf or NaN
            {
                out.mantissa = in.mantissa << (Float32MantissaBits - Uf11MantissaBits);
                out.exponent = Float32ExponentMask;
            }
            else // Normalized
            {
                out.mantissa = in.mantissa << (Float32MantissaBits - Uf11MantissaBits);
                out.exponent = (Float32ExponentMask >> 1) - (Uf11ExponentMask >> 1) + in.exponent;
            }
        }
        return out;
    }
} // namespace

float toFloat(u16 data)
{
    Uf11 in;
    in.as_u16 = data;

    Float32 out = Uf11ToFloatImpl(in);
    return out.as_float;
}
} // namespace uf11
