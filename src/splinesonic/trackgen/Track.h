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

#include "TrackGenExport.h"

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

    using TrackSkeletonNodeArray = std::vector<TrackSkeletonNode>;

    struct TrackSkeleton
    {
        GenerationInfo genInfo;
        TrackSkeletonNodeArray nodes;
    };

    REAPER_TRACKGEN_API void GenerateTrackSkeleton(const GenerationInfo& genInfo, TrackSkeleton& track);
}}
