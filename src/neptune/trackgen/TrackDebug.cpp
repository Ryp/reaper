////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TrackDebug.h"

#include "Track.h"

#include <core/Assert.h>

#include "math/Spline.h"

namespace Neptune
{
void write_track_skeleton_as_obj(std::ostream& output, std::vector<TrackSkeletonNode>& skeleton)
{
    output << "o Skeleton" << std::endl;

    for (const auto& node : skeleton)
    {
        output << 'v';
        output << ' ' << node.position_ws.x;
        output << ' ' << node.position_ws.y;
        output << ' ' << node.position_ws.z;
        output << std::endl;
    }

    for (u32 i = 1; i < skeleton.size(); i++)
    {
        output << 'l';
        output << ' ' << i;
        output << ' ' << i + 1;
        output << std::endl;
    }
}

void write_track_splines_as_obj(std::ostream& output, std::vector<TrackSkeletonNode>& skeleton,
                                std::vector<Reaper::Math::Spline>& splines, u32 tesselation)
{
    const u32 splineCount = static_cast<u32>(splines.size());

    output << "o Splines" << std::endl;

    Assert(tesselation > 0);
    Assert(splineCount > 0);

    for (u32 i = 0; i < splineCount; i++)
    {
        const Reaper::Math::Spline& spline = splines[i];
        const TrackSkeletonNode&    node = skeleton[i];
        for (u32 j = 0; j < (tesselation + 1); j++)
        {
            const float     param = static_cast<float>(j) / static_cast<float>(tesselation);
            const glm::vec3 pos_ms = Reaper::Math::spline_eval(spline, param);
            const glm::vec3 pos_ws = node.position_ws + node.orientation_ms_to_ws * pos_ms;

            output << 'v';
            output << ' ' << pos_ws.x;
            output << ' ' << pos_ws.y;
            output << ' ' << pos_ws.z;
            output << std::endl;
        }
    }

    for (u32 i = 0; i < splineCount * tesselation; i++)
    {
        output << 'l';
        output << ' ' << i + 1;
        output << ' ' << i + 2;
        output << std::endl;
    }
}

void write_track_bones_as_obj(std::ostream& output, std::vector<TrackSkinning>& skinningInfo)
{
    const u32 trackChunkCount = static_cast<u32>(skinningInfo.size());

    Assert(trackChunkCount > 0);

    const u32 bonesPerChunkCount = static_cast<u32>(skinningInfo[0].bones.size());

    output << "o Bones" << std::endl;
    for (const auto& skinning : skinningInfo)
    {
        Assert(bonesPerChunkCount == skinning.bones.size());

        for (const auto& bone : skinning.bones)
        {
            {
                const glm::vec3 pos = bone.root;

                output << 'v';
                output << ' ' << pos.x;
                output << ' ' << pos.y;
                output << ' ' << pos.z;
                output << std::endl;
            }
            {
                const glm::vec3 pos = bone.end;

                output << 'v';
                output << ' ' << pos.x;
                output << ' ' << pos.y;
                output << ' ' << pos.z;
                output << std::endl;
            }
        }
    }

    for (u32 i = 0; i < trackChunkCount * bonesPerChunkCount; i++)
    {
        output << 'l';
        output << ' ' << i + 1;
        output << ' ' << i + 2;
        output << std::endl;
    }
}
} // namespace Neptune
