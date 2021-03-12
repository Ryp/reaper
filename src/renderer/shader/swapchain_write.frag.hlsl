#include "lib/base.hlsl"

#include "lib/color_space.hlsl"
#include "share/swapchain.hlsl"

VK_BINDING(0, 0) ConstantBuffer<SwapchainPassParams> pass_params;
VK_BINDING(1, 0) SamplerState linear_sampler;
VK_BINDING(2, 0) Texture2D<float3> t_hdr_color;

// NOTE: VK_FORMAT_B8G8R8A8_SRGB takes care of the conversion for us.
#define ENABLE_MANUAL_SRGB_CONVERSION 0

struct PS_INPUT
{
    float4 PositionCS : SV_Position;
    float2 PositionUV : TEXCOORD0;
};

struct PS_OUTPUT
{
    float3 color : SV_Target0;
};

PS_OUTPUT main(PS_INPUT input)
{
    float3 linear_hdr_color = t_hdr_color.SampleLevel(linear_sampler, input.PositionUV, 0);

    linear_hdr_color *= pass_params.dummy_boost;

    PS_OUTPUT output;

#if ENABLE_MANUAL_SRGB_CONVERSION
    output.color = linear_to_srgb(linear_hdr_color);
#else
    output.color = linear_hdr_color;
#endif

    return output;
}
