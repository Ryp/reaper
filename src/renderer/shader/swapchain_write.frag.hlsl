#include "lib/base.hlsl"

#include "lib/color_space.hlsl"

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

PS_OUTPUT main(PS_INPUT input)
{
    float3 linear_hdr_color = t_hdr_color.SampleLevel(linear_sampler, input.PositionUV, 0);

    linear_hdr_color *= pass_params.dummy_boost;
    linear_hdr_color *= 0.001;

    PS_OUTPUT output;

    if (spec_color_space == COLOR_SPACE_REC2020)
        linear_hdr_color = rec709_to_rec2020(linear_hdr_color);

    output.color = apply_transfer_func(linear_hdr_color, spec_transfer_function);

    return output;
}
