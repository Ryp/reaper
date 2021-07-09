////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TrackDebug.h"

#include "Track.h"

#include "math/Spline.h"

namespace SplineSonic::TrackGen
{
void SaveTrackSkeletonAsObj(std::ostream& output, std::vector<TrackSkeletonNode>& skeleton)
{
    output << "o Skeleton" << std::endl;

    for (const auto& node : skeleton)
    {
        output << 'v';
        output << ' ' << node.positionWS.x;
        output << ' ' << node.positionWS.y;
        output << ' ' << node.positionWS.z;
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

void SaveTrackSplinesAsObj(std::ostream& output, std::vector<TrackSkeletonNode>& skeleton,
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
            const glm::vec3 posMS = Reaper::Math::EvalSpline(spline, param);
            const glm::vec3 posWS = node.positionWS + node.orientationWS * posMS;

            output << 'v';
            output << ' ' << posWS.x;
            output << ' ' << posWS.y;
            output << ' ' << posWS.z;
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

void SaveTrackBonesAsObj(std::ostream& output, std::vector<TrackSkinning>& skinningInfo)
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
} // namespace SplineSonic::TrackGen
