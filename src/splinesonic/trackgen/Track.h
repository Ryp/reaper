////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Reaper { namespace Math
{
    struct Spline;
}}

namespace SplineSonic { namespace TrackGen
{
    struct GenerationInfo
    {
        u32 length;
        float width;
        float chaos;
    };

    struct TrackSkeletonNode
    {
        glm::vec3 positionWS;
        float radius;
        glm::quat inOrientation;
        glm::quat outOrientation;
        float inWidth;
        float outWidth;
    };

    struct Bone
    {
        glm::vec3 boneRootMS;
        glm::vec3 boneEndMS;
    };

    struct TrackSkinning
    {
        std::vector<Bone> bones;
    };

    struct Track
    {
        GenerationInfo genInfo;
        std::vector<TrackSkeletonNode> skeletonNodes;
        std::vector<Reaper::Math::Spline> splines;
        std::vector<TrackSkinning> skinning;
    };

    REAPER_TRACKGEN_API void GenerateTrackSkeleton(const GenerationInfo& genInfo, std::vector<TrackSkeletonNode>& skeletonNodes);

    REAPER_TRACKGEN_API void GenerateTrackSplines(const std::vector<TrackSkeletonNode>& skeletonNodes, std::vector<Reaper::Math::Spline>& splines);

    REAPER_TRACKGEN_API void GenerateTrackSkinning(const std::vector<Reaper::Math::Spline>& splines, std::vector<TrackSkinning>& skinning);
}} // namespace SplineSonic::TrackGen
