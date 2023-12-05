////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vulkan_loader/Vulkan.h>

#include <vector>

namespace Reaper
{
struct ShaderModules;

using PipelineFunctor = VkPipeline (*)(VkDevice device, const ShaderModules& shader_modules,
                                       VkPipelineLayout pipeline_layout);

struct PipelineCreator
{
    VkPipelineLayout pipeline_layout;
    PipelineFunctor  pipeline_creation_function;
};

struct PipelineTracker
{
    PipelineCreator creator;
    u32             loaded_version;
    u32             pipeline_index;

    static constexpr u32 InvalidIndex = 0xffffffff;
};

struct PipelineFactory
{
    std::vector<PipelineTracker> trackers;
    std::vector<VkPipeline>      pipelines;
    bool                         dirty;
};

struct VulkanBackend;
PipelineFactory create_pipeline_factory(VulkanBackend& backend);
void            destroy_pipeline_factory(VulkanBackend& backend, PipelineFactory& pipeline_factory);

// Returns a tracker index
u32 register_pipeline_creator(PipelineFactory& factory, const PipelineCreator& creator);

void pipeline_factory_update(VulkanBackend& backend, PipelineFactory& pipeline_factory, ShaderModules& shader_modules);

VkPipeline get_pipeline(const PipelineFactory& pipeline_factory, u32 pipeline_tracker_index);
} // namespace Reaper
