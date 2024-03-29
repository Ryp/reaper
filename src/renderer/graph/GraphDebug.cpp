////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GraphDebug.h"

#include <fstream>

#include <fmt/format.h>

#include "renderer/vulkan/Image.h"                      // FIXME
#include "renderer/vulkan/api/VulkanStringConversion.h" // FIXME

namespace Reaper::FrameGraph
{
void DumpFrameGraph(const FrameGraph& frameGraph)
{
    std::ofstream outResFile("resource.txt");
    std::ofstream outRenderPassFile("renderpass.txt");
    std::ofstream outRenderPassSEFile("renderpass-sideeffect.txt");
    std::ofstream outRPInput("input.txt");
    std::ofstream outRPOutput("output.txt");

    const u32 renderPassCount = static_cast<u32>(frameGraph.RenderPasses.size());
    const u32 resourceUsageCount = static_cast<u32>(frameGraph.ResourceUsages.size());

    for (u32 resourceUsageIndex = 0; resourceUsageIndex < resourceUsageCount; resourceUsageIndex++)
    {
        const ResourceUsage& resourceUsage = frameGraph.ResourceUsages[resourceUsageIndex];
        const ResourceHandle resource_handle = resourceUsage.resource_handle;
        const Resource&      resource = GetResource(frameGraph, resourceUsage);

        // Instead of skipping read nodes, add the hidden info to the corresponding edge
        if (has_mask(resourceUsage.type, UsageType::Output))
        {
            std::string label;

            if (resource_handle.is_texture)
            {
                const GPUTextureProperties& desc = resource.properties.texture;
                label = fmt::format("{0} ({1})\\n{2}x{3} {6}\\n{4}\\n[{5}]", resource.debug_name, resource_handle.index,
                                    desc.width, desc.height, vk_to_string(PixelFormatToVulkan(desc.format)),
                                    resourceUsageIndex,
                                    desc.sample_count > 1 ? vk_to_string(SampleCountToVulkan(desc.sample_count)) : "");
            }
            else
            {
                const GPUBufferProperties& desc = resource.properties.buffer;
                label = fmt::format("{0} ({1})\\n{2}x{3} bytes\\n[{4}]", resource.debug_name, resource_handle.index,
                                    desc.element_count, desc.element_size_bytes, resourceUsageIndex);
            }

            outResFile << "        res" << resourceUsageIndex << " [label=\"" << label << "\"";

            if (!resourceUsage.is_used)
                outResFile << ",fillcolor=\"#BBBBBB\"";

            outResFile << "]" << std::endl;
        }
    }

    for (u32 renderPassIndex = 0; renderPassIndex < renderPassCount; renderPassIndex++)
    {
        const RenderPass& renderPass = frameGraph.RenderPasses[renderPassIndex];
        std::ostream&     output = (renderPass.has_side_effects ? outRenderPassSEFile : outRenderPassFile);

        const std::string label = fmt::format("{0} [{1}]", renderPass.debug_name, renderPassIndex);

        output << "        pass" << renderPassIndex << " [label=\"" << label << "\"";

        if (!renderPass.is_used)
            output << ",fillcolor=\"#BBBBBB\"";
        output << "]" << std::endl;

        for (auto& resourceUsageHandle : renderPass.ResourceUsageHandles)
        {
            const ResourceUsage& resourceUsage = GetResourceUsage(frameGraph, resourceUsageHandle);
            if (has_mask(resourceUsage.type, UsageType::Input))
            {
                outRPInput << "        res" << resourceUsage.parent_usage_handle << " -> pass" << renderPassIndex
                           << std::endl;
            }

            if (has_mask(resourceUsage.type, UsageType::Output))
            {
                outRPOutput << "        pass" << renderPassIndex << " -> res" << resourceUsageHandle << std::endl;
            }
        }
    }
}
} // namespace Reaper::FrameGraph
