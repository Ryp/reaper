////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_DEBUG_INCLUDED
#define LIB_DEBUG_INCLUDED

float3 heatmap_color(float value)
{
    const uint g_color_count = 5;

    const float3 g_colors[g_color_count] = {
        float3(0.0, 0.0, 1.0),
        float3(0.0, 1.0, 1.0),
        float3(0.0, 1.0, 0.0),
        float3(1.0, 1.0, 0.0),
        float3(1.0, 0.0, 0.0)
    };

    const float epsilon = 0.0001;
    value = min(max(value, epsilon), 1.0 - epsilon);

    const float index = value * (float)(g_color_count - 1);

    const uint color_index = trunc(index);
    const float t = frac(index);

    const float3 color_a = g_colors[color_index];
    const float3 color_b = g_colors[color_index + 1];

    return lerp(color_a, color_b, t);
}

#endif
