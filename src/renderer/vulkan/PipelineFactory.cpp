////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PipelineFactory.h"

#include "Backend.h"
#include "core/Assert.h"

namespace Reaper
{
PipelineFactory create_pipeline_factory(VulkanBackend& backend)
{
    static_cast<void>(backend);

    return {
        .trackers = {},
        .pipelines = {},
        .dirty = true,
    };
}

void destroy_pipeline_factory(VulkanBackend& backend, PipelineFactory& pipeline_factory)
{
    for (auto pipeline : pipeline_factory.pipelines)
    {
        vkDestroyPipeline(backend.device, pipeline, nullptr);
    }

    pipeline_factory.pipelines.clear();
}

u32 register_pipeline_creator(PipelineFactory& factory, const PipelineCreator& creator)
{
    const u32 index = static_cast<u32>(factory.trackers.size());

    factory.trackers.emplace_back(PipelineTracker{
        .creator = creator,
        .loaded_version = 0,
        .pipeline_index = PipelineTracker::InvalidIndex,
    });

    return index;
}

namespace
{
    VkPipeline create_new_pipeline_from_tracker(VkDevice device, PipelineTracker& tracker,
                                                ShaderModules& shader_modules)
    {
        // tracker
        VkPipeline pipeline =
            tracker.creator.pipeline_creation_function(device, shader_modules, tracker.creator.pipeline_layout);

        tracker.loaded_version += 1;

        return pipeline;
    }
} // namespace

void pipeline_factory_update(VulkanBackend& backend, PipelineFactory& pipeline_factory, ShaderModules& shader_modules)
{
    if (pipeline_factory.dirty)
    {
        for (auto& tracker : pipeline_factory.trackers)
        {
            if (tracker.loaded_version == 0)
            {
                VkPipeline pipeline = create_new_pipeline_from_tracker(backend.device, tracker, shader_modules);

                tracker.pipeline_index = static_cast<u32>(pipeline_factory.pipelines.size());

                pipeline_factory.pipelines.push_back(pipeline);
            }
        }

        pipeline_factory.dirty = false;
    }
}

VkPipeline get_pipeline(const PipelineFactory& pipeline_factory, u32 pipeline_tracker_index)
{
    const PipelineTracker& tracker = pipeline_factory.trackers[pipeline_tracker_index];
    Assert(tracker.loaded_version != 0);

    return pipeline_factory.pipelines[tracker.pipeline_index];
}
} // namespace Reaper
