////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <doctest/doctest.h>

#include "renderer/graph/FrameGraph.h"
#include "renderer/graph/FrameGraphBuilder.h"

#include "renderer/graph/GraphDebug.h"

#include <fstream>

namespace Reaper
{
namespace FrameGraph
{
    namespace
    {
        struct GBufferPassOutput
        {
            ResourceUsageHandle GBufferRT;
        };

        struct ShadowPassOutput
        {
            ResourceUsageHandle ShadowRT;
        };

        struct LightingPassOutput
        {
            ResourceUsageHandle OpaqueRT;
        };

        struct CompositePassOutput
        {
            ResourceUsageHandle BackBufferRT;
        };

        void ShadowPass(FrameGraphBuilder& builder, ShadowPassOutput& output)
        {
            const RenderPassHandle shadowPass = builder.create_render_pass("Shadow");

            // Outputs
            {
                GPUTextureProperties shadowRTDesc =
                    DefaultGPUTextureProperties(512, 512, PixelFormat::R16G16B16A16_UNORM);

                TGPUTextureUsage shadowRTUsage = {};

                const ResourceUsageHandle shadowRTHandle =
                    builder.create_texture(shadowPass, "VSM", shadowRTDesc, shadowRTUsage);
                output.ShadowRT = shadowRTHandle;
            }
        }

        void GBufferPass(FrameGraphBuilder& builder, GBufferPassOutput& output)
        {
            const RenderPassHandle gbufferPass = builder.create_render_pass("GBuffer");

            // Outputs
            {
                GPUTextureProperties gbufferRTDesc =
                    DefaultGPUTextureProperties(1280, 720, PixelFormat::R16G16B16A16_UNORM);

                TGPUTextureUsage gbufferUsage = {};

                const ResourceUsageHandle gbufferRTHandle =
                    builder.create_texture(gbufferPass, "GBuffer", gbufferRTDesc, gbufferUsage);
                output.GBufferRT = gbufferRTHandle;
            }
        }

        void UselessPass(FrameGraphBuilder& builder, const GBufferPassOutput& gBufferInput)
        {
            const RenderPassHandle uselessPass = builder.create_render_pass("PruneMe");

            // Inputs
            {
                TGPUTextureUsage gbufferRTUsage = {};

                builder.read_texture(uselessPass, gBufferInput.GBufferRT, gbufferRTUsage);
            }

            // Outputs
            {
                GPUTextureProperties uselessRTDesc =
                    DefaultGPUTextureProperties(4096, 4096, PixelFormat::R16G16_SFLOAT);
                uselessRTDesc.sampleCount = 4;

                TGPUTextureUsage uselessRTUsage = {};

                const ResourceUsageHandle uselessRTHandle =
                    builder.create_texture(uselessPass, "UselessTexture", uselessRTDesc, uselessRTUsage);

                (void)uselessRTHandle; // FIXME
            };
        }

        void LightingPass(FrameGraphBuilder& builder, LightingPassOutput& output, const ShadowPassOutput& shadowOutput,
                          const GBufferPassOutput& gBufferInput)
        {
            const RenderPassHandle lightingPass = builder.create_render_pass("Lighting");

            // Inputs
            {
                TGPUTextureUsage gbufferRTUsage = {};

                TGPUTextureUsage shadowRTUsage = {};

                builder.read_texture(lightingPass, gBufferInput.GBufferRT, gbufferRTUsage);
                builder.read_texture(lightingPass, shadowOutput.ShadowRT, shadowRTUsage);
            }

            // Outputs
            {
                GPUTextureProperties opaqueRTDesc =
                    DefaultGPUTextureProperties(1280, 720, PixelFormat::R16G16B16A16_SFLOAT);

                TGPUTextureUsage opaqueRTUsage = {};

                const ResourceUsageHandle opaqueRTHandle =
                    builder.create_texture(lightingPass, "Opaque", opaqueRTDesc, opaqueRTUsage);
                output.OpaqueRT = opaqueRTHandle;
            }
        }

        void CompositePass(FrameGraphBuilder& builder, CompositePassOutput& output,
                           const LightingPassOutput& lightingOutput)
        {
            const RenderPassHandle compositePass = builder.create_render_pass("Composite");

            // Inputs
            {
                TGPUTextureUsage opaqueRTUsage = {};

                builder.read_texture(compositePass, lightingOutput.OpaqueRT, opaqueRTUsage);
            }

            // Outputs
            {
                GPUTextureProperties backBufferRTDesc =
                    DefaultGPUTextureProperties(1280, 720, PixelFormat::R8G8B8_SRGB);

                TGPUTextureUsage backBufferRTUsage = {};

                const ResourceUsageHandle backBufferRTHandle =
                    builder.create_texture(compositePass, "BackBuffer", backBufferRTDesc, backBufferRTUsage);
                output.BackBufferRT = backBufferRTHandle;
            }
        }

        void PresentPass(FrameGraphBuilder& builder, const CompositePassOutput& compositeOutput)
        {
            const bool             HasSideEffects = true;
            const RenderPassHandle presentPass = builder.create_render_pass("Present", HasSideEffects);

            // Inputs
            {
                TGPUTextureUsage backBufferRTUsage = {};

                builder.read_texture(presentPass, compositeOutput.BackBufferRT, backBufferRTUsage);
            }
        }

        void RecordFrame(FrameGraphBuilder& builder)
        {
            GBufferPassOutput   gbufferOut = {};
            ShadowPassOutput    shadowOut = {};
            LightingPassOutput  lightingOut = {};
            CompositePassOutput compositeOut = {};

            ShadowPass(builder, shadowOut);
            GBufferPass(builder, gbufferOut);
            UselessPass(builder, gbufferOut);
            LightingPass(builder, lightingOut, shadowOut, gbufferOut);
            CompositePass(builder, compositeOut, lightingOut);
            PresentPass(builder, compositeOut);
        }
    } // namespace
} // namespace FrameGraph
} // namespace Reaper

using namespace Reaper::FrameGraph;

TEST_CASE("Frame Graph")
{
    FrameGraph        frameGraph;
    FrameGraphBuilder builder(frameGraph);

    RecordFrame(builder);

    builder.build();

    // DumpFrameGraph(frameGraph);
}
