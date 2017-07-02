////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TrackDebug.h"

#include "Track.h"

#include "math/Spline.h"

namespace SplineSonic { namespace TrackGen
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

    void SaveTrackSplinesAsObj(std::ostream& output, std::vector<Reaper::Math::Spline>& splines, u32 tesselation)
    {
        const u32 splineCount = splines.size();

        output << "o Splines" << std::endl;

        Assert(tesselation > 0);

        for (const auto& spline : splines)
        {
            for (u32 i = 0; i < (tesselation + 1); i++)
            {
                float param = static_cast<float>(i) / static_cast<float>(tesselation);
                glm::vec3 pos = Reaper::Math::EvalSpline(spline, param);

                output << 'v';
                output << ' ' << pos.x;
                output << ' ' << pos.y;
                output << ' ' << pos.z;
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
        const u32 trackChunkCount = skinningInfo.size();

        Assert(trackChunkCount > 0);

        const u32 bonesPerChunkCount = skinningInfo[0].bones.size();

        output << "o Bones" << std::endl;
        for (const auto& skinning : skinningInfo)
        {
            Assert(bonesPerChunkCount == skinning.bones.size());

            for (const auto& bone : skinning.bones)
            {
                {
                    glm::vec3 pos = bone.boneRootMS;

                    output << 'v';
                    output << ' ' << pos.x;
                    output << ' ' << pos.y;
                    output << ' ' << pos.z;
                    output << std::endl;
                }
                {
                    glm::vec3 pos = bone.boneEndMS;

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
}}
