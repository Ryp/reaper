#include "lib/base.hlsl"

#include "lib/color_space.hlsl"
#include "lib/tranfer_functions.hlsl"
#include "lib/tonemapping.hlsl"

#include "swapchain_write.share.hlsl"

VK_CONSTANT(0) const uint spec_color_space = 0;
VK_CONSTANT(1) const uint spec_transfer_function = 0;
VK_CONSTANT(2) const uint spec_dynamic_range = 0;
VK_CONSTANT(3) const uint spec_tonemap_function = 0;

VK_PUSH_CONSTANT_HELPER(SwapchainWriteParams) Consts;

VK_BINDING(0, 0) SamplerState linear_sampler;
VK_BINDING(0, 1) Texture2D<float3> t_hdr_scene;
VK_BINDING(0, 2) Texture2D<float3> Lighting;
VK_BINDING(0, 3) Texture2D<float4> t_ldr_gui;
VK_BINDING(0, 4) ByteAddressBuffer AvgLog2Luminance;
VK_BINDING(0, 5) Texture2D<float3> t_ldr_debug;

struct PS_INPUT
{
    float4 PositionCS : SV_Position;
    float2 PositionUV : TEXCOORD0;
};

struct PS_OUTPUT
{
    float3 color : SV_Target0;
};

float3 composite_sdr_ui_in_hdr(float3 scene_color_hdr_nits, float4 sdr_gui_color, float sdr_ui_max_brightness_nits)
{
    // FIXME naive version
    // This style of blending will burn the UI opacity gradient when the background is bright.
    return lerp(scene_color_hdr_nits, sdr_gui_color.rgb * sdr_ui_max_brightness_nits, sdr_gui_color.a);
}

float3 apply_tonemapping_operator(float3 color_scene_nits, uint tonemap_function, float min_nits, float max_nits)
{
    // FIXME Use min_nits and make this a proper display mapping
    if (tonemap_function == TONEMAP_FUNC_LINEAR)
        return color_scene_nits;
    else if (tonemap_function == TONEMAP_FUNC_UNCHARTED2)
        return tonemapping_uncharted2(color_scene_nits / max_nits) * max_nits;
    else if (tonemap_function == TONEMAP_FUNC_ACES_APPROX)
        return tonemapping_filmic_aces(color_scene_nits / max_nits) * max_nits;
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

float3 apply_sdr_transfer_func(float3 color_normalized, uint transfer_function)
{
    if (transfer_function == TRANSFER_FUNC_LINEAR)
        return color_normalized;
    else if (transfer_function == TRANSFER_FUNC_SRGB)
        return linear_to_srgb(color_normalized);
    else if (transfer_function == TRANSFER_FUNC_REC709)
        return linear_to_rec709(color_normalized);
    else
        return 0.42; // Invalid
}

float3 apply_hdr_transfer_func(float3 color_linear_nits, uint transfer_function)
{
    if (transfer_function == TRANSFER_FUNC_PQ)
        return linear_to_pq(color_linear_nits / PQ_MAX_NITS);
    else if (transfer_function == TRANSFER_FUNC_WINDOWS_SCRGB)
        return color_linear_nits / 80.f;
    else
        return 0.42; // Invalid
}

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    // Unexposed scene color in linear sRGB
    float3 color = t_hdr_scene.SampleLevel(linear_sampler, input.PositionUV, 0);

    if (input.PositionUV.x > 0.5)
    {
        const float3 lighting = Lighting.SampleLevel(linear_sampler, input.PositionUV, 0);
        color = lighting;
    }

#define USE_AUTO_EXPOSURE 0
#if USE_AUTO_EXPOSURE
    float average_log2_luma = asfloat(AvgLog2Luminance.Load(0));
    float exposure = exp2(-average_log2_luma) * 0.18;
#else
    // FIXME The HDR range at this point should preferably be in nits, so that we can set
    // an exposure key in more consistent units as well. Here it's completely arbitrary
    // but it gives us an exposed image.
    const float manual_exposure_key = 200.0;
    float exposure = manual_exposure_key;
#endif

    // Apply exposure
    color *= exposure * Consts.exposure_compensation;

    // Composite UI
    float4 sdr_gui_color = t_ldr_gui.SampleLevel(linear_sampler, input.PositionUV, 0);
    color = composite_sdr_ui_in_hdr(color, sdr_gui_color, Consts.sdr_ui_max_brightness_nits);

    // Apply tone mapping
    color = apply_tonemapping_operator(color, spec_tonemap_function, Consts.tonemap_min_nits, Consts.tonemap_max_nits);

#define SHOW_DEBUG_TEXTURE 0
#if SHOW_DEBUG_TEXTURE
    // FIXME Blend in debug color
    const float3 ldr_debug_color = t_ldr_debug.SampleLevel(linear_sampler, input.PositionUV, 0);

    color = lerp(color, ldr_debug_color * Consts.sdr_peak_brightness_nits, 0.25);
#endif

    color = apply_output_color_space_transform(color, spec_color_space);

    if (spec_dynamic_range == DYNAMIC_RANGE_SDR)
    {
        float3 color_normalized = color / Consts.sdr_peak_brightness_nits;

        output.color = apply_sdr_transfer_func(color_normalized, spec_transfer_function);
    }
    else if (spec_dynamic_range == DYNAMIC_RANGE_HDR)
    {
        output.color = apply_hdr_transfer_func(color, spec_transfer_function);
    }
}

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
