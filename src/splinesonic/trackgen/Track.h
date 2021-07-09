////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Reaper::Math
{
struct Spline;
}

struct Mesh;

namespace SplineSonic::TrackGen
{
struct GenerationInfo
{
    u32   length;
    float width;
    float chaos;
};

struct TrackSkeletonNode
{
    glm::vec3 positionWS;
    float     radius;
    glm::quat orientationWS;
    glm::quat rotationLS; // Deviation of the chunk in local space
    float     inWidth;
    float     outWidth;
};

struct Bone
{
    glm::vec3 root;
    glm::vec3 end;
};

struct TrackSkinning
{
    std::vector<Bone>       bones;
    std::vector<glm::fmat4> invBindTransforms;
    std::vector<glm::fmat4> poseTransforms;
};

struct Track
{
    GenerationInfo                    genInfo;
    std::vector<TrackSkeletonNode>    skeletonNodes;
    std::vector<Reaper::Math::Spline> splinesMS;
    std::vector<TrackSkinning>        skinning;
};

REAPER_TRACKGEN_API
void GenerateTrackSkeleton(const GenerationInfo& genInfo, std::vector<TrackSkeletonNode>& skeletonNodes);

REAPER_TRACKGEN_API
void GenerateTrackSplines(const std::vector<TrackSkeletonNode>& skeletonNodes,
                          std::vector<Reaper::Math::Spline>&    splines);

REAPER_TRACKGEN_API
void GenerateTrackSkinning(const std::vector<TrackSkeletonNode>&    skeletonNodes,
                           const std::vector<Reaper::Math::Spline>& splines,
                           std::vector<TrackSkinning>&              skinning);

REAPER_TRACKGEN_API
void SkinTrackChunkMesh(const TrackSkeletonNode& node,
                        const TrackSkinning&     trackSkinning,
                        Mesh&                    mesh,
                        float                    meshLength);
} // namespace SplineSonic::TrackGen
