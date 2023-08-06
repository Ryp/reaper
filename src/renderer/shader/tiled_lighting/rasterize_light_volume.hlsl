////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef RASTERIZE_LIGHT_VOLUME_INCLUDED
#define RASTERIZE_LIGHT_VOLUME_INCLUDED

#include "tiled_lighting.share.hlsl"

struct PassParams
{
    LightVolumeInstance instance_array[LightVolumeMax];
};

VK_PUSH_CONSTANT_HELPER(TileLightRasterPushConstants) consts;

VK_BINDING(0, 0) ConstantBuffer<PassParams> pass_params;
VK_BINDING(0, 1) Texture2D<float> TileDepth;
VK_BINDING(0, 2) globallycoherent RWByteAddressBuffer TileVisibleLightIndices;
VK_BINDING(0, 3) ByteAddressBuffer VertexPositionsMS;

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 position_cs : TEXCOORD0; // Without SV_POSITION semantics
    nointerpolation uint instance_id : TEXCOORD1;
};

#endif
