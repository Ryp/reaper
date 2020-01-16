#include "GraphDebug.h"

#include <fstream>

#include <fmt/format.h>

#include "renderer/vulkan/Image.h"                      // FIXME
#include "renderer/vulkan/api/VulkanStringConversion.h" // FIXME

namespace Reaper
{
const char* GetLoadOpString(ELoadOp loadOp)
{
    switch (loadOp)
    {
    case ELoadOp::Load:
        return "Load";
    case ELoadOp::Clear:
        return "Clear";
    case ELoadOp::DontCare:
        return "DontCare";
    default:
        AssertUnreachable();
        return "ERROR";
    }
}

const char* GetStoreOpString(EStoreOp storeOp)
{
    switch (storeOp)
    {
    case EStoreOp::Store:
        return "Store";
    case EStoreOp::DontCare:
        return "DontCare";
    default:
        AssertUnreachable();
        return "ERROR";
    }
}

const char* GetImageLayoutString(EImageLayout layout)
{
    switch (layout)
    {
    case EImageLayout::General:
        return "General";
    case EImageLayout::ColorAttachmentOptimal:
        return "ColorAttachmentOptimal";
    case EImageLayout::DepthStencilAttachmentOptimal:
        return "DepthStencilAttachmentOptimal";
    case EImageLayout::DepthStencilReadOnlyOptimal:
        return "DepthStencilReadOnlyOptimal";
    case EImageLayout::ShaderReadOnlyOptimal:
        return "ShaderReadOnlyOptimal";
    case EImageLayout::TransferSrcOptimal:
        return "TransferSrcOptimal";
    case EImageLayout::TransferDstOptimal:
        return "TransferDstOptimal";
    case EImageLayout::Preinitialized:
        return "Preinitialized";
    default:
        AssertUnreachable();
        return "ERROR";
    }
}

const char* GetFormatString(PixelFormat format)
{
    switch (format)
    {
    default:
        AssertUnreachable();
        return "ERROR";
    }
}
} // namespace Reaper

namespace Reaper
{
namespace FrameGraph
{
    void DumpFrameGraph(const FrameGraph& frameGraph)
    {
        std::ofstream outResFile("resource.txt");
        std::ofstream outRenderPassFile("renderpass.txt");
        std::ofstream outRenderPassSEFile("renderpass-sideeffect.txt");
        std::ofstream outRPInput("input.txt");
        std::ofstream outRPOutput("output.txt");

        const u32 renderPassCount = frameGraph.RenderPasses.size();
        const u32 resourceUsageCount = frameGraph.ResourceUsages.size();

        for (u32 resourceUsageIndex = 0; resourceUsageIndex < resourceUsageCount; resourceUsageIndex++)
        {
            const ResourceUsage&        resourceUsage = frameGraph.ResourceUsages[resourceUsageIndex];
            const Resource&             resource = frameGraph.Resources[resourceUsage.Resource];
            const GPUTextureProperties& desc = resource.Descriptor;

            // Instead of skipping a node, add the hidden info to the corresponding edge
            if (resourceUsage.Type == UsageType::RenderPassInput)
                continue;

            const std::string label = fmt::format("{0} ({1})\\n{2}x{3}\\n{4}\\n{5} Cube={6} [{7}]", resource.Identifier,
                                                  resourceUsage.Resource, desc.width, desc.height,
                                                  GetFormatToString(PixelFormatToVulkan(desc.format)), desc.sampleCount,
                                                  desc.miscFlags & GPUMiscFlags::Cubemap, resourceUsageIndex);

            outResFile << "        res" << resourceUsageIndex << " [label=\"" << label << "\"";

            if (!resourceUsage.IsUsed)
                outResFile << ",fillcolor=\"#BBBBBB\"";

            outResFile << "]" << std::endl;
        }

        for (u32 renderPassIndex = 0; renderPassIndex < renderPassCount; renderPassIndex++)
        {
            const RenderPass& renderPass = frameGraph.RenderPasses[renderPassIndex];
            std::ostream&     output = (renderPass.HasSideEffects ? outRenderPassSEFile : outRenderPassFile);

            const std::string label = fmt::format("{0} [{1}]", renderPass.Identifier, renderPassIndex);

            output << "        pass" << renderPassIndex << " [label=\"" << label << "\"";

            if (!renderPass.IsUsed)
                output << ",fillcolor=\"#BBBBBB\"";
            output << "]" << std::endl;

            for (auto& resourceUsageHandle : renderPass.ResourceUsageHandles)
            {
                const ResourceUsage& resourceUsage = GetResourceUsage(frameGraph, resourceUsageHandle);
                if (resourceUsage.Type == UsageType::RenderPassInput)
                    outRPInput << "        res" << resourceUsage.Parent << " -> pass" << renderPassIndex << std::endl;
                else if (resourceUsage.Type == UsageType::RenderPassOutput)
                    outRPOutput << "        pass" << renderPassIndex << " -> res" << resourceUsageHandle << std::endl;
            }
        }
    }
} // namespace FrameGraph
} // namespace Reaper
