#include "lib/base.hlsl"

#include "lib/color_space.hlsl"
#include "lib/eotf.hlsl"

#include "share/color_space.hlsl"
#include "share/swapchain.hlsl"

VK_CONSTANT(0) const uint spec_transfer_function = 0;
VK_CONSTANT(1) const uint spec_color_space = 0;

VK_BINDING(0, 0) ConstantBuffer<SwapchainPassParams> pass_params;
VK_BINDING(1, 0) SamplerState linear_sampler;
VK_BINDING(2, 0) Texture2D<float3> t_hdr_color;

struct PS_INPUT
{
    float4 PositionCS : SV_Position;
    float2 PositionUV : TEXCOORD0;
};

struct PS_OUTPUT
{
    float3 color : SV_Target0;
};

float3 apply_transfer_func(float3 color, uint transfer_function)
{
    if (spec_transfer_function == TRANSFER_FUNC_LINEAR)
        return color;
    else if (spec_transfer_function == TRANSFER_FUNC_SRGB)
        return srgb_eotf(color);
    else if (spec_transfer_function == TRANSFER_FUNC_REC709)
        return rec709_eotf(color);
    else if (spec_transfer_function == TRANSFER_FUNC_PQ)
        return pq_eotf(color);
    else
        return 0.42; // Invalid
}

float3 apply_transfer_func_inverse(float3 color, uint transfer_function)
{
    if (spec_transfer_function == TRANSFER_FUNC_LINEAR)
        return color;
    else if (spec_transfer_function == TRANSFER_FUNC_SRGB)
        return srgb_eotf_inverse(color);
    else if (spec_transfer_function == TRANSFER_FUNC_REC709)
        return rec709_eotf_inverse(color);
    else if (spec_transfer_function == TRANSFER_FUNC_PQ)
        return pq_eotf_inverse(color);
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

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    float3 scene_color_srgb_linear = t_hdr_color.SampleLevel(linear_sampler, input.PositionUV, 0);

    scene_color_srgb_linear *= pass_params.dummy_boost;

    float3 scene_color_linear = apply_output_color_space_transform(scene_color_srgb_linear, spec_color_space);

    float3 display_color = apply_transfer_func(scene_color_linear, spec_transfer_function);

    output.color = display_color;
}
