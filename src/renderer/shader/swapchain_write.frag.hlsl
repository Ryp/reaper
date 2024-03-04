#include "lib/base.hlsl"

#include "lib/color_space.hlsl"
#include "lib/eotf.hlsl"
#include "lib/tonemapping.hlsl"

#include "swapchain_write.share.hlsl"

VK_CONSTANT(0) const uint spec_transfer_function = 0;
VK_CONSTANT(1) const uint spec_color_space = 0;
VK_CONSTANT(2) const uint spec_dynamic_range = 0;
VK_CONSTANT(3) const uint spec_tonemap_function = 0;

VK_PUSH_CONSTANT_HELPER(SwapchainWriteParams) Consts;

VK_BINDING(0, 0) SamplerState linear_sampler;
VK_BINDING(0, 1) Texture2D<float3> t_hdr_scene;
VK_BINDING(0, 2) Texture2D<float3> Lighting;
VK_BINDING(0, 3) Texture2D<float4> t_ldr_gui;
VK_BINDING(0, 4) Texture2D<float3> t_ldr_debug;

struct PS_INPUT
{
    float4 PositionCS : SV_Position;
    float2 PositionUV : TEXCOORD0;
};

struct PS_OUTPUT
{
    float3 color : SV_Target0;
};

float3 apply_sdr_transfer_func(float3 color_normalized, uint transfer_function)
{
    if (transfer_function == TRANSFER_FUNC_LINEAR)
        return color_normalized;
    else if (transfer_function == TRANSFER_FUNC_SRGB)
        return srgb_eotf(color_normalized);
    else if (transfer_function == TRANSFER_FUNC_REC709)
        return rec709_eotf(color_normalized);
    else
        return 0.42; // Invalid
}

float3 apply_hdr_transfer_func(float3 color_linear_nits, uint transfer_function)
{
    if (transfer_function == TRANSFER_FUNC_PQ)
        return pq_eotf(color_linear_nits / PQ_MAX_NITS);
    else if (transfer_function == TRANSFER_FUNC_WINDOWS_SCRGB)
        return color_linear_nits / 80.f;
    else
        return 0.42; // Invalid
}

float3 apply_output_color_space_transform(float3 color_srgb, uint color_space)
{
    if (color_space == COLOR_SPACE_SRGB)
        return color_srgb;
    else if (color_space == COLOR_SPACE_REC709)
        return srgb_to_rec709(color_srgb);
    else if (color_space == COLOR_SPACE_DISPLAY_P3)
        return srgb_to_display_p3(color_srgb);
    else if (color_space == COLOR_SPACE_REC2020)
        return srgb_to_rec2020(color_srgb);
    else
        return 0.4242; // Invalid
}

float3 apply_tonemapping_operator(float3 color, uint tonemap_function)
{
    if (tonemap_function == TONEMAP_FUNC_NONE)
        return color;
    else if (tonemap_function == TONEMAP_FUNC_UNCHARTED2)
        return tonemapping_uncharted2(color);
    else if (tonemap_function == TONEMAP_FUNC_ACES_APPROX)
        return tonemapping_filmic_aces(color);
    else
        return 0.42; // Invalid
}

static const float exposure = 1.f; // FIXME

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    // Unexposed scene color in linear sRGB
    float3 color = t_hdr_scene.SampleLevel(linear_sampler, input.PositionUV, 0);

    if (input.PositionUV.x > 0.5)
    {
        const float3 lighting = Lighting.SampleLevel(linear_sampler, input.PositionUV, 0);
        color = lighting;
    }

    color *= exposure;
    color = apply_tonemapping_operator(color, spec_tonemap_function);

    if (false)
    {
        // FIXME Blend in debug color
        const float3 ldr_debug_color = t_ldr_debug.SampleLevel(linear_sampler, input.PositionUV, 0);

        color = lerp(color, ldr_debug_color, 0.25);
    }

    float4 ldr_gui_color = t_ldr_gui.SampleLevel(linear_sampler, input.PositionUV, 0);

    // Blend in GUI
    // FIXME This is wrong:
    // - dynamic range (SDR vs HDR and paperwhite constants)
    // - alpha blending
    // - probably more
    color = lerp(color, ldr_gui_color.rgb, ldr_gui_color.a);

    // Display transforms
    color = apply_output_color_space_transform(color, spec_color_space);

#define DEBUG_CIE_DIAGRAM 0
#if DEBUG_CIE_DIAGRAM
    float2 cie_xy = float2(input.PositionUV.x, 1.0 - input.PositionUV.y);

    if (spec_transfer_function == TRANSFER_FUNC_PQ)
    {
        color = cie_xy_to_rec2020(cie_xy);

        if (any(color < 0.0))
        {
            color = 0.0;
        }

        color *= 80.0 / PQ_MAX_NITS;
    }
    else
    {
        color = cie_xy_to_sRGB(cie_xy);
        if (any(color < 0.0))
        {
            color = 0;
        }
    }
#endif

#define DEBUG_SIDE_GRADIENT 0
#if DEBUG_SIDE_GRADIENT
    if (input.PositionUV.x > 0.97)
    {
        color = 1.0 - input.PositionUV.y;
    }
#endif

    // FIXME It's convenient to have 1.f mapped to 400 nits at this stage since it makes the GUI behave nicely without needing to properly support HDR.
    // Normally we would get nits before tonemapping and without this ugly hack.
    float3 color_linear_nits = color * 400.f;

    if (spec_dynamic_range == DYNAMIC_RANGE_SDR)
    {
        float3 color_normalized = color_linear_nits / Consts.sdr_peak_brightness_nits;

        output.color = apply_sdr_transfer_func(color_normalized, spec_transfer_function);
    }
    else if (spec_dynamic_range == DYNAMIC_RANGE_HDR)
    {
        output.color = apply_hdr_transfer_func(color_linear_nits, spec_transfer_function);
    }
}