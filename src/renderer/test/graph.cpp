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
                    DefaultGPUTextureProperties(512, 512, PixelFormat::R16G16B16A16_UNORM);

                GPUResourceUsage shadowRTUsage = {};

                const ResourceUsageHandle shadowRTHandle =
                    builder.create_texture(shadowPass, "VSM", shadowRTDesc, shadowRTUsage);
                output.ShadowRT = shadowRTHandle;
            }
        }

        void GBufferPass(Builder& builder, GBufferPassOutput& output)
        {
            const RenderPassHandle gbufferPass = builder.create_render_pass("GBuffer");

            // Outputs
            {
                GPUTextureProperties gbufferRTDesc =
                    DefaultGPUTextureProperties(1280, 720, PixelFormat::R16G16B16A16_UNORM);

                GPUResourceUsage gbufferUsage = {};

                const ResourceUsageHandle gbufferRTHandle =
                    builder.create_texture(gbufferPass, "GBuffer", gbufferRTDesc, gbufferUsage);
                output.GBufferRT = gbufferRTHandle;
            }
        }

        void UselessPass(Builder& builder, const GBufferPassOutput& gBufferInput)
        {
            const RenderPassHandle uselessPass = builder.create_render_pass("PruneMe");

            // Inputs
            {
                GPUResourceUsage gbufferRTUsage = {};

                builder.read_texture(uselessPass, gBufferInput.GBufferRT, gbufferRTUsage);
            }

            // Outputs
            {
                GPUTextureProperties uselessRTDesc =
                    DefaultGPUTextureProperties(4096, 4096, PixelFormat::R16G16_SFLOAT);
                uselessRTDesc.sampleCount = 4;

                GPUResourceUsage uselessRTUsage = {};

                const ResourceUsageHandle uselessRTHandle =
                    builder.create_texture(uselessPass, "UselessTexture", uselessRTDesc, uselessRTUsage);

                static_cast<void>(uselessRTHandle);
            };
        }

        void LightingPass(Builder& builder, LightingPassOutput& output, const ShadowPassOutput& shadowOutput,
                          const GBufferPassOutput& gBufferInput)
        {
            const RenderPassHandle lightingPass = builder.create_render_pass("Lighting");

            // Inputs
            {
                GPUResourceUsage gbufferRTUsage = {};

                GPUResourceUsage shadowRTUsage = {};

                builder.read_texture(lightingPass, gBufferInput.GBufferRT, gbufferRTUsage);
                builder.read_texture(lightingPass, shadowOutput.ShadowRT, shadowRTUsage);
            }

            // Outputs
            {
                GPUTextureProperties opaqueRTDesc =
                    DefaultGPUTextureProperties(1280, 720, PixelFormat::R16G16B16A16_SFLOAT);

                GPUResourceUsage opaqueRTUsage = {};

                const ResourceUsageHandle opaqueRTHandle =
                    builder.create_texture(lightingPass, "Opaque", opaqueRTDesc, opaqueRTUsage);
                output.OpaqueRT = opaqueRTHandle;
            }
        }

        void CompositePass(Builder& builder, CompositePassOutput& output, const LightingPassOutput& lightingOutput)
        {
            const RenderPassHandle compositePass = builder.create_render_pass("Composite");

            // Inputs
            {
                GPUResourceUsage opaqueRTUsage = {};

                builder.read_texture(compositePass, lightingOutput.OpaqueRT, opaqueRTUsage);
            }

            // Outputs
            {
                GPUTextureProperties backBufferRTDesc =
                    DefaultGPUTextureProperties(1280, 720, PixelFormat::R8G8B8_SRGB);

                GPUResourceUsage backBufferRTUsage = {};

                const ResourceUsageHandle backBufferRTHandle =
                    builder.create_texture(compositePass, "BackBuffer", backBufferRTDesc, backBufferRTUsage);
                output.BackBufferRT = backBufferRTHandle;
            }
        }

        void PresentPass(Builder& builder, const CompositePassOutput& compositeOutput)
        {
            const bool             has_side_effects = true;
            const RenderPassHandle presentPass = builder.create_render_pass("Present", has_side_effects);

            // Inputs
            {
                GPUResourceUsage backBufferRTUsage = {};

                builder.read_texture(presentPass, compositeOutput.BackBufferRT, backBufferRTUsage);
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
