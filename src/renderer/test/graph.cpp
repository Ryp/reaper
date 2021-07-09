////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "pch/stdafx.h"
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
            const RenderPassHandle shadowPass = builder.CreateRenderPass("Shadow");

            // Outputs
            {
                GPUTextureProperties shadowRTDesc =
                    DefaultGPUTextureProperties(512, 512, PixelFormat::R16G16B16A16_UNORM);

                TGPUTextureUsage shadowRTUsage = {};
                shadowRTUsage.LoadOp = ELoadOp::Clear;
                shadowRTUsage.Layout = EImageLayout::ColorAttachmentOptimal;

                const ResourceUsageHandle shadowRTHandle =
                    builder.CreateTexture(shadowPass, "VSM", shadowRTDesc, shadowRTUsage);
                output.ShadowRT = shadowRTHandle;
            }
        }

        void GBufferPass(FrameGraphBuilder& builder, GBufferPassOutput& output)
        {
            const RenderPassHandle gbufferPass = builder.CreateRenderPass("GBuffer");

            // Outputs
            {
                GPUTextureProperties gbufferRTDesc =
                    DefaultGPUTextureProperties(1280, 720, PixelFormat::R16G16B16A16_UNORM);

                TGPUTextureUsage gbufferUsage = {};
                gbufferUsage.LoadOp = ELoadOp::DontCare;
                gbufferUsage.Layout = EImageLayout::ColorAttachmentOptimal;

                const ResourceUsageHandle gbufferRTHandle =
                    builder.CreateTexture(gbufferPass, "GBuffer", gbufferRTDesc, gbufferUsage);
                output.GBufferRT = gbufferRTHandle;
            }
        }

        void UselessPass(FrameGraphBuilder& builder, const GBufferPassOutput& gBufferInput)
        {
            const RenderPassHandle uselessPass = builder.CreateRenderPass("PruneMe");

            // Inputs
            {
                TGPUTextureUsage gbufferRTUsage = {};
                gbufferRTUsage.Layout = EImageLayout::ShaderReadOnlyOptimal;

                builder.ReadTexture(uselessPass, gBufferInput.GBufferRT, gbufferRTUsage);
            }

            // Outputs
            {
                GPUTextureProperties uselessRTDesc =
                    DefaultGPUTextureProperties(4096, 4096, PixelFormat::R16G16_SFLOAT);
                uselessRTDesc.sampleCount = 4;

                TGPUTextureUsage uselessRTUsage = {};
                uselessRTUsage.LoadOp = ELoadOp::DontCare;
                uselessRTUsage.Layout = EImageLayout::ColorAttachmentOptimal;

                const ResourceUsageHandle uselessRTHandle =
                    builder.CreateTexture(uselessPass, "UselessTexture", uselessRTDesc, uselessRTUsage);

                (void)uselessRTHandle; // FIXME
            };
        }

        void LightingPass(FrameGraphBuilder& builder, LightingPassOutput& output, const ShadowPassOutput& shadowOutput,
                          const GBufferPassOutput& gBufferInput)
        {
            const RenderPassHandle lightingPass = builder.CreateRenderPass("Lighting");

            // Inputs
            {
                TGPUTextureUsage gbufferRTUsage = {};
                gbufferRTUsage.Layout = EImageLayout::ShaderReadOnlyOptimal;

                TGPUTextureUsage shadowRTUsage = {};
                shadowRTUsage.Layout = EImageLayout::ShaderReadOnlyOptimal;

                builder.ReadTexture(lightingPass, gBufferInput.GBufferRT, gbufferRTUsage);
                builder.ReadTexture(lightingPass, shadowOutput.ShadowRT, shadowRTUsage);
            }

            // Outputs
            {
                GPUTextureProperties opaqueRTDesc =
                    DefaultGPUTextureProperties(1280, 720, PixelFormat::R16G16B16A16_SFLOAT);

                TGPUTextureUsage opaqueRTUsage = {};
                opaqueRTUsage.LoadOp = ELoadOp::DontCare;
                opaqueRTUsage.Layout = EImageLayout::ColorAttachmentOptimal;

                const ResourceUsageHandle opaqueRTHandle =
                    builder.CreateTexture(lightingPass, "Opaque", opaqueRTDesc, opaqueRTUsage);
                output.OpaqueRT = opaqueRTHandle;
            }
        }

        void CompositePass(FrameGraphBuilder& builder, CompositePassOutput& output,
                           const LightingPassOutput& lightingOutput)
        {
            const RenderPassHandle compositePass = builder.CreateRenderPass("Composite");

            // Inputs
            {
                TGPUTextureUsage opaqueRTUsage = {};
                opaqueRTUsage.Layout = EImageLayout::ColorAttachmentOptimal;

                builder.ReadTexture(compositePass, lightingOutput.OpaqueRT, opaqueRTUsage);
            }

            // Outputs
            {
                GPUTextureProperties backBufferRTDesc =
                    DefaultGPUTextureProperties(1280, 720, PixelFormat::R8G8B8_SRGB);

                TGPUTextureUsage backBufferRTUsage = {};
                backBufferRTUsage.LoadOp = ELoadOp::DontCare;
                backBufferRTUsage.Layout = EImageLayout::ColorAttachmentOptimal;

                const ResourceUsageHandle backBufferRTHandle =
                    builder.CreateTexture(compositePass, "BackBuffer", backBufferRTDesc, backBufferRTUsage);
                output.BackBufferRT = backBufferRTHandle;
            }
        }

        void PresentPass(FrameGraphBuilder& builder, const CompositePassOutput& compositeOutput)
        {
            const bool             HasSideEffects = true;
            const RenderPassHandle presentPass = builder.CreateRenderPass("Present", HasSideEffects);

            // Inputs
            {
                TGPUTextureUsage backBufferRTUsage = {};
                backBufferRTUsage.Layout = EImageLayout::ColorAttachmentOptimal;

                builder.ReadTexture(presentPass, compositeOutput.BackBufferRT, backBufferRTUsage);
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

    builder.Build();

    // DumpFrameGraph(frameGraph);
}
