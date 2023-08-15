////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include <doctest/doctest.h>

#include "renderer/graph/FrameGraph.h"
#include "renderer/graph/FrameGraphBuilder.h"

#include "renderer/graph/GraphDebug.h"

#include <fstream>

namespace Reaper
{
const u32 dummy_usage_flags = 0;

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

        void ShadowPass(Builder& builder, ShadowPassOutput& output)
        {
            const RenderPassHandle shadowPass = builder.create_render_pass("Shadow");

            // Outputs
            {
                GPUTextureProperties shadowRTDesc =
                    default_texture_properties(512, 512, PixelFormat::R16G16B16A16_UNORM, dummy_usage_flags);

                GPUTextureAccess shadowRTAccess = {};

                const ResourceUsageHandle shadowRTHandle =
                    builder.create_texture(shadowPass, "VSM", shadowRTDesc, shadowRTAccess);
                output.ShadowRT = shadowRTHandle;
            }
        }

        void GBufferPass(Builder& builder, GBufferPassOutput& output)
        {
            const RenderPassHandle gbufferPass = builder.create_render_pass("GBuffer");

            // Outputs
            {
                GPUTextureProperties gbufferRTDesc =
                    default_texture_properties(1280, 720, PixelFormat::R16G16B16A16_UNORM, dummy_usage_flags);

                GPUTextureAccess gbufferAccess = {};

                const ResourceUsageHandle gbufferRTHandle =
                    builder.create_texture(gbufferPass, "GBuffer", gbufferRTDesc, gbufferAccess);
                output.GBufferRT = gbufferRTHandle;
            }
        }

        void UselessPass(Builder& builder, const GBufferPassOutput& gBufferInput)
        {
            const RenderPassHandle uselessPass = builder.create_render_pass("PruneMe");

            // Inputs
            {
                GPUTextureAccess gbufferRTAccess = {};

                builder.read_texture(uselessPass, gBufferInput.GBufferRT, gbufferRTAccess);
            }

            // Outputs
            {
                GPUTextureProperties uselessRTDesc =
                    default_texture_properties(4096, 4096, PixelFormat::R16G16_SFLOAT, dummy_usage_flags);
                uselessRTDesc.sample_count = 4;

                GPUTextureAccess uselessRTAccess = {};

                const ResourceUsageHandle uselessRTHandle =
                    builder.create_texture(uselessPass, "UselessTexture", uselessRTDesc, uselessRTAccess);

                static_cast<void>(uselessRTHandle);
            };
        }

        void LightingPass(Builder& builder, LightingPassOutput& output, const ShadowPassOutput& shadowOutput,
                          const GBufferPassOutput& gBufferInput)
        {
            const RenderPassHandle lightingPass = builder.create_render_pass("Lighting");

            // Inputs
            {
                GPUTextureAccess gbufferRTAccess = {};

                GPUTextureAccess shadowRTAccess = {};

                builder.read_texture(lightingPass, gBufferInput.GBufferRT, gbufferRTAccess);
                builder.read_texture(lightingPass, shadowOutput.ShadowRT, shadowRTAccess);
            }

            // Outputs
            {
                GPUTextureProperties opaqueRTDesc =
                    default_texture_properties(1280, 720, PixelFormat::R16G16B16A16_SFLOAT, dummy_usage_flags);

                GPUTextureAccess opaqueRTAccess = {};

                const ResourceUsageHandle opaqueRTHandle =
                    builder.create_texture(lightingPass, "Opaque", opaqueRTDesc, opaqueRTAccess);
                output.OpaqueRT = opaqueRTHandle;
            }
        }

        void CompositePass(Builder& builder, CompositePassOutput& output, const LightingPassOutput& lightingOutput)
        {
            const RenderPassHandle compositePass = builder.create_render_pass("Composite");

            // Inputs
            {
                GPUTextureAccess opaqueRTAccess = {};

                builder.read_texture(compositePass, lightingOutput.OpaqueRT, opaqueRTAccess);
            }

            // Outputs
            {
                GPUTextureProperties backBufferRTDesc =
                    default_texture_properties(1280, 720, PixelFormat::R8G8B8_SRGB, dummy_usage_flags);

                GPUTextureAccess backBufferRTAccess = {};

                const ResourceUsageHandle backBufferRTHandle =
                    builder.create_texture(compositePass, "BackBuffer", backBufferRTDesc, backBufferRTAccess);
                output.BackBufferRT = backBufferRTHandle;
            }
        }

        void PresentPass(Builder& builder, const CompositePassOutput& compositeOutput)
        {
            const bool             has_side_effects = true;
            const RenderPassHandle presentPass = builder.create_render_pass("Present", has_side_effects);

            // Inputs
            {
                GPUTextureAccess backBufferRTAccess = {};

                builder.read_texture(presentPass, compositeOutput.BackBufferRT, backBufferRTAccess);
            }
        }

        void RecordFrame(Builder& builder)
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
    FrameGraph frameGraph;
    Builder    builder(frameGraph);

    RecordFrame(builder);

    builder.build();

    // DumpFrameGraph(frameGraph);
}
