#include "lib/base.hlsl"

#include "lib/color_space.hlsl"
#include "lib/eotf.hlsl"
#include "lib/tonemapping.hlsl"

#include "share/color_space.hlsl"
#include "share/swapchain.hlsl"

VK_CONSTANT(0) const uint spec_transfer_function = 0;
VK_CONSTANT(1) const uint spec_color_space = 0;
VK_CONSTANT(2) const uint spec_tonemap_function = 0;

VK_BINDING(0, 0) ConstantBuffer<SwapchainPassParams> pass_params;
VK_BINDING(1, 0) SamplerState linear_sampler;
VK_BINDING(2, 0) Texture2D<float3> t_hdr_scene;
VK_BINDING(3, 0) Texture2D<float4> t_ldr_gui;

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
    if (transfer_function == TRANSFER_FUNC_LINEAR)
        return color;
    else if (transfer_function == TRANSFER_FUNC_SRGB)
        return srgb_eotf(color);
    else if (transfer_function == TRANSFER_FUNC_REC709)
        return rec709_eotf(color);
    else if (transfer_function == TRANSFER_FUNC_PQ)
        return pq_eotf(color);
    else
        return 0.42; // Invalid
}

float3 apply_transfer_func_inverse(float3 color, uint transfer_function)
{
    if (transfer_function == TRANSFER_FUNC_LINEAR)
        return color;
    else if (transfer_function == TRANSFER_FUNC_SRGB)
        return srgb_eotf_inverse(color);
    else if (transfer_function == TRANSFER_FUNC_REC709)
        return rec709_eotf_inverse(color);
    else if (transfer_function == TRANSFER_FUNC_PQ)
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

float3 apply_tonemapping_operator(float3 color, uint tonemap_function)
{
    if (tonemap_function == TONEMAP_FUNC_NONE)
        return color;
    else if (tonemap_function == TONEMAP_FUNC_UNCHARTED2)
        return tonemapping_uncharted2(color);
    else if (tonemap_function == TONEMAP_FUNC_ACES)
        return tonemapping_filmic_aces(color);
    else
        return 0.42; // Invalid
}

float3 cie_xy_to_XYZ(float2 cie_xy)
{
    const float2 cie = cie_xy;
    return 0.5 * float3(cie.x / cie.y, 1.0, (1.0 - cie.x - cie.y) / cie.y);
}

float3 cie_xy_to_sRGB(float2 cie_xy)
{
    static const float3x3 XYZ_to_sRGB_matrix =
    {
        3.2409699419, -1.5373831776, -0.4986107603,
       -0.9692436363,  1.8759675015,  0.0415550574,
        0.0556300797, -0.2039769589,  1.0569715142,
    };

    float3 cie_XYZ = cie_xy_to_XYZ(cie_xy);

    return mul(XYZ_to_sRGB_matrix, cie_XYZ);
}

float3 cie_xy_to_rec2020(float2 cie_xy)
{
    static const float3x3 XYZ_to_rec2020_matrix =
    {
        1.7166511880, -0.3556707838, -0.2533662814,
    -0.6666843518,  1.6164812366,  0.0157685458,
        0.0176398574, -0.0427706133,  0.9421031212,
    };

    float3 cie_XYZ = cie_xy_to_XYZ(cie_xy);
    float3 color_rec2020 = mul(XYZ_to_rec2020_matrix, cie_XYZ);
    float maxChannel = max(color_rec2020.r, max(color_rec2020.g, color_rec2020.b));
    return color_rec2020 / maxChannel;
}

static const float exposure = 1.f; // FIXME

void main(in PS_INPUT input, out PS_OUTPUT output)
{
    // Unexposed scene color in linear sRGB
    float3 color = t_hdr_scene.SampleLevel(linear_sampler, input.PositionUV, 0);
    float4 ldr_gui_color = t_ldr_gui.SampleLevel(linear_sampler, input.PositionUV, 0);

    color *= exposure;
    color = apply_tonemapping_operator(color, spec_tonemap_function);

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

    color = apply_transfer_func(color, spec_transfer_function);

    output.color = color;
}
